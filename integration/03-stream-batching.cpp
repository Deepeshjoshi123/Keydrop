/// 03-stream-batching.cpp — Stream optimizer: reuse + batching
///
/// What you learn:
///  1. Enable the stream optimizer
///  2. Identical payloads → packet reuse  (no bytes on the wire)
///  3. Low-change payloads → queued into batch envelopes
///  4. flush_stream() emits any pending batched packets
///
/// Build:  see CMakeLists.txt in this directory
/// Run:    ./build/integration/03-stream-batching

#include <cassert>
#include <iostream>
#include <string>

#include "keydrop/schema/schema_runtime.hpp"

using namespace keydrop;

int main()
{
    // ── 1. Schema + runtime with stream optimizer enabled ──────
    SchemaDef schema{"Metric", 10, {
        FieldDef{"sensor",  FieldType::string, 0, FieldConstraints{true, 32}},
        FieldDef{"value",   FieldType::u32,     1, {}},
        FieldDef{"unit",    FieldType::string, 2, FieldConstraints{true, 8}},
    }};

    SchemaRuntime runtime;
    runtime.register_schema(schema);

    StreamOptimizerConfig stream_cfg;
    stream_cfg.enabled                    = true;
    stream_cfg.enable_packet_reuse        = true;
    stream_cfg.enable_batching            = true;
    stream_cfg.aggressive_after_samples   = 4;
    stream_cfg.max_batch_packets          = 4;
    stream_cfg.low_change_ratio_threshold = 0.35f;
    runtime.set_stream_optimizer_config(stream_cfg);

    // ── 2. Build a payload ─────────────────────────────────────
    NamedPayload p;
    p["sensor"] = FieldValue::from_string("temp-01");
    p["value"]  = FieldValue::from_u32(225);
    p["unit"]   = FieldValue::from_string("celsius");

    // ── 3. Send the same payload twice — second is reused ──────
    std::cout << "Sending identical payloads...\n";
    for (int i = 0; i < 2; ++i) {
        Buffer out;
        bool has_packet = false;
        runtime.send_stream("Metric", p, out, has_packet);

        if (has_packet) {
            std::cout << "  msg " << i << ": emitted " << out.size() << " B\n";
        } else {
            std::cout << "  msg " << i << ": packet REUSED (no bytes emitted)\n";
        }
    }

    // ── 4. Slightly different payload — batched ────────────────
    std::cout << "\nSending low-change payloads (queued for batching)...\n";
    p["value"] = FieldValue::from_u32(226);   // only value changed
    for (int i = 2; i < 6; ++i) {
        Buffer out;
        bool has_packet = false;
        runtime.send_stream("Metric", p, out, has_packet);
        std::cout << "  msg " << i << ": " << (has_packet ? "emitted" : "queued") << "\n";
    }

    // ── 5. Flush the batch ─────────────────────────────────────
    Buffer batch;
    bool has_batch = false;
    runtime.flush_stream(batch, has_batch);
    if (has_batch) {
        std::cout << "\n[flush] batch packet: " << batch.size() << " B"
                  << " (contains multiple queued messages)\n";

        // Decode the batch
        std::vector<std::pair<std::string, NamedPayload>> messages;
        SchemaRuntimeResult res = runtime.receive_stream(batch, messages);
        assert(res.ok());
        std::cout << "[flush] decoded " << messages.size()
                  << " messages from the batch\n";
    }

    std::cout << "\n[pass] stream optimizer: reuse + batching working\n";
    return 0;
}
