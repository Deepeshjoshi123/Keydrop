#pragma once

#include <deque>
#include <string>
#include <unordered_map>

#include "keydrop/core/buffer.hpp"
#include "keydrop/schema/field_mapper.hpp"
#include "keydrop/schema/schema_types.hpp"

namespace keydrop {

struct StreamOptimizerConfig {
    bool enabled = false;
    bool enable_packet_reuse = true;
    bool enable_delta_updates = true;
    bool enable_batching = true;
    // Phase 3: stateful delta packets — presence bitmap (3B) plus signed
    // numeric deltas (3C) with per-schema sequence numbers and periodic
    // keyframes. Opt-in: disabled by default so existing stateless and
    // batched behavior is unchanged unless the caller enables it.
    bool enable_delta_packets = false;
    usize keyframe_interval = 100; // full packet every N records per schema
    usize aggressive_after_samples = 8;
    usize max_batch_packets = 4;
    f32 low_change_ratio_threshold = 0.35f;
};

struct StreamOptimizationOutput {
    bool emit_now = true;
    bool used_packet_reuse = false;
    bool queued_for_batch = false;
    bool aggressive_mode = false;
    bool used_delta_packet = false;
    bool keyframe = false;
    Buffer packet;
};

class StreamOptimizer {
public:
    StreamOptimizer();

    void configure(const StreamOptimizerConfig& config);
    const StreamOptimizerConfig& config() const;
    void reset();

    // The schema is needed only when enable_delta_packets is on (field types).
    void optimize_outgoing(
        const SchemaDef& schema,
        const std::string& schema_name,
        const NamedPayload& payload,
        const Buffer& encoded_packet,
        StreamOptimizationOutput& out
    );

    bool flush_batched(Buffer& out_packet);
    bool expand_incoming(const Buffer& packet, std::deque<Buffer>& out_packets) const;

    // Phase 3 delta expansion. Pure read of receiver state: it rebuilds the
    // full stateless packet from the last decoded payload for the schema.
    // Returns false when the packet is not a valid delta for the current
    // state (missing keyframe state or sequence mismatch) — the caller must
    // resynchronize by waiting for the next full packet (keyframe). A
    // rejected delta is never partially decoded.
    bool expand_delta(
        const SchemaDef& schema,
        const Buffer& packet,
        Buffer& out_full_packet,
        NamedPayload& out_merged_payload
    ) const;

    // Commit receiver state after a successfully decoded full packet.
    // Any full packet resynchronizes the delta sequence counter.
    void record_decoded(const std::string& schema_name, const NamedPayload& payload);

    // Commit receiver state after a successfully decoded delta packet:
    // advances the expected delta sequence.
    void record_decoded_delta(const std::string& schema_name, const NamedPayload& payload);

    static constexpr byte kDeltaMarker = 0xFB;    // stateful delta packet
    static constexpr byte kControlMarker = 0xFA;  // stream control packet

private:
    static constexpr byte kBatchMarker = 0xFC;

    static bool payload_equals(const NamedPayload& a, const NamedPayload& b);
    static bool field_changed(
        const NamedPayload& previous,
        const NamedPayload& current,
        const std::string& name
    );
    static f32 change_ratio(const NamedPayload& previous, const NamedPayload& current);
    static Buffer build_batch_packet(const std::deque<Buffer>& packets);

    bool build_delta_packet(
        const SchemaDef& schema,
        const NamedPayload& previous,
        const NamedPayload& current,
        u16 sequence,
        Buffer& out
    ) const;

    StreamOptimizerConfig config_;
    std::unordered_map<std::string, NamedPayload> last_payload_by_schema_;
    std::unordered_map<std::string, Buffer> last_packet_by_schema_;
    std::unordered_map<std::string, usize> sample_count_by_schema_;
    std::deque<Buffer> batched_packets_;
    // Phase 3 delta state (sender side)
    std::unordered_map<std::string, u16> delta_seq_by_schema_;
    std::unordered_map<std::string, usize> record_count_by_schema_;
    // Phase 3 delta state (receiver side). Kept separate from the sender
    // payload history so one runtime instance can round-trip its own deltas.
    std::unordered_map<std::string, NamedPayload> last_decoded_by_schema_;
    std::unordered_map<std::string, u16> expected_delta_seq_by_schema_;
};

}
