#pragma once

#include <deque>
#include <string>
#include <unordered_map>

#include "keydrop/core/buffer.hpp"
#include "keydrop/schema/field_mapper.hpp"

namespace keydrop {

struct StreamOptimizerConfig {
    bool enabled = false;
    bool enable_packet_reuse = true;
    bool enable_delta_updates = true;
    bool enable_batching = true;
    usize aggressive_after_samples = 8;
    usize max_batch_packets = 4;
    f32 low_change_ratio_threshold = 0.35f;
};

struct StreamOptimizationOutput {
    bool emit_now = true;
    bool used_packet_reuse = false;
    bool queued_for_batch = false;
    bool aggressive_mode = false;
    Buffer packet;
};

class StreamOptimizer {
public:
    StreamOptimizer();

    void configure(const StreamOptimizerConfig& config);
    const StreamOptimizerConfig& config() const;
    void reset();

    void optimize_outgoing(
        const std::string& schema_name,
        const NamedPayload& payload,
        const Buffer& encoded_packet,
        StreamOptimizationOutput& out
    );

    bool flush_batched(Buffer& out_packet);
    bool expand_incoming(const Buffer& packet, std::deque<Buffer>& out_packets) const;

private:
    static constexpr byte kBatchMarker = 0xFC;

    static bool payload_equals(const NamedPayload& a, const NamedPayload& b);
    static f32 change_ratio(const NamedPayload& previous, const NamedPayload& current);
    static Buffer build_batch_packet(const std::deque<Buffer>& packets);

    StreamOptimizerConfig config_;
    std::unordered_map<std::string, NamedPayload> last_payload_by_schema_;
    std::unordered_map<std::string, Buffer> last_packet_by_schema_;
    std::unordered_map<std::string, usize> sample_count_by_schema_;
    std::deque<Buffer> batched_packets_;
};

}
