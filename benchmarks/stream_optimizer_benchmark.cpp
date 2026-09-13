#include <chrono>
#include <iostream>

#include "keydrop/schema/schema_runtime.hpp"

using namespace keydrop;

int main()
{
    SchemaRuntime runtime;
    const SchemaDef schema {
        "BenchmarkData",
        77,
        {
            FieldDef {"value", FieldType::u16, 0, {}},
            FieldDef {"device_id", FieldType::string, 1, FieldConstraints {true, 32}},
        }
    };
    if (!runtime.register_schema(schema).ok())
    {
        return 1;
    }

    StreamOptimizerConfig stream_cfg;
    stream_cfg.enabled = true;
    stream_cfg.enable_packet_reuse = true;
    stream_cfg.enable_delta_updates = true;
    stream_cfg.enable_batching = true;
    stream_cfg.aggressive_after_samples = 4;
    stream_cfg.max_batch_packets = 6;
    stream_cfg.low_change_ratio_threshold = 0.5f;
    runtime.set_stream_optimizer_config(stream_cfg);
    runtime.reset_stream_optimizer();

    const usize total_messages = 10000;
    usize emitted_packets = 0;
    usize emitted_bytes = 0;
    usize decoded_messages = 0;

    const auto start = std::chrono::steady_clock::now();
    for (usize i = 0; i < total_messages; ++i)
    {
        NamedPayload payload;
        payload["value"] = FieldValue::from_u16(static_cast<u16>(1000 + (i % 5)));
        payload["device_id"] = FieldValue::from_string("bench_sensor");

        Buffer out;
        bool has_packet = false;
        if (!runtime.send_stream("BenchmarkData", payload, out, has_packet).ok())
        {
            return 1;
        }

        if (has_packet)
        {
            emitted_packets += 1;
            emitted_bytes += out.size();

            std::vector<std::pair<std::string, NamedPayload>> decoded;
            if (!runtime.receive_stream(out, decoded).ok())
            {
                return 1;
            }
            decoded_messages += decoded.size();
        }
    }

    Buffer flush_packet;
    bool flushed = false;
    runtime.flush_stream(flush_packet, flushed);
    if (flushed)
    {
        emitted_packets += 1;
        emitted_bytes += flush_packet.size();
        std::vector<std::pair<std::string, NamedPayload>> decoded;
        if (!runtime.receive_stream(flush_packet, decoded).ok())
        {
            return 1;
        }
        decoded_messages += decoded.size();
    }

    const auto end = std::chrono::steady_clock::now();
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    const double seconds = static_cast<double>(micros) / 1000000.0;

    std::cout << "messages=" << total_messages << "\n";
    std::cout << "decoded_messages=" << decoded_messages << "\n";
    std::cout << "emitted_packets=" << emitted_packets << "\n";
    std::cout << "emitted_bytes=" << emitted_bytes << "\n";
    std::cout << "throughput_msg_per_sec=" << (seconds > 0.0 ? total_messages / seconds : 0.0) << "\n";
    std::cout << "avg_latency_us_per_message=" << (total_messages > 0 ? static_cast<double>(micros) / total_messages : 0.0) << "\n";

    return decoded_messages == total_messages ? 0 : 1;
}
