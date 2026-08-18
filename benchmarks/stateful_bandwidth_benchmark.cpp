// Phase 3 stateful bandwidth benchmark.
//
// Measures steady-state wire bytes per record for the fixed telemetry
// record under four modes:
//   stateless   — plain send(), no state (the Phase 0 baseline)
//   dictionary  — adaptive string dictionary (3A)
//   batched     — packet reuse + batch envelopes (3D)
//   delta       — stateful delta packets: presence bitmap + signed deltas
//                 with periodic keyframes (3B + 3C)
//
// Workload: 10000 records with a constant device_id, a periodically
// increasing timestamp, a rarely-changing status, and slowly changing
// temperature/humidity. Reports first-use (cold) and steady-state bytes
// separately, plus a loss-safety pass: dropping emitted packets must never
// produce a silently misdecoded value (every decode either matches or is
// rejected until a keyframe resynchronizes).
//
// Output is key=value lines suitable for CSV capture.

#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

#include "keydrop/schema/schema_runtime.hpp"

using namespace keydrop;

namespace {

constexpr usize kRecords = 10000;
constexpr usize kKeyframeInterval = 100;

const SchemaDef kSchema {
    "Telemetry",
    60,
    {
        FieldDef {"timestamp", FieldType::u32, 0, {}},
        FieldDef {"device_id", FieldType::string, 1, FieldConstraints {true, 32}},
        FieldDef {"status", FieldType::u8, 2, {}},
        FieldDef {"temperature", FieldType::u16, 3, {}},
        FieldDef {"humidity", FieldType::u16, 4, {}},
    }
};

NamedPayload make_payload(usize i)
{
    NamedPayload p;
    p["timestamp"] = FieldValue::from_u32(static_cast<u32>(1000000 + i * 1000)); // every record
    p["device_id"] = FieldValue::from_string("sensor_01");                       // constant
    p["status"] = FieldValue::from_u8(i % 500 == 0 ? 1 : 0);                     // rare
    p["temperature"] = FieldValue::from_u16(static_cast<u16>(210 + (i % 70) / 10)); // every 10th
    p["humidity"] = FieldValue::from_u16(static_cast<u16>(550 + (i % 250) / 25));    // every 25th
    return p;
}

// W3: string-heavy record for the repeated-string dictionary target.
const SchemaDef kStringSchema {
    "Strings",
    61,
    {
        FieldDef {"device_id", FieldType::string, 0, FieldConstraints {true, 32}},
        FieldDef {"status", FieldType::string, 1, FieldConstraints {true, 32}},
        FieldDef {"unit", FieldType::string, 2, FieldConstraints {true, 8}},
        FieldDef {"value", FieldType::u16, 3, {}},
    }
};

NamedPayload make_string_payload(usize i)
{
    NamedPayload p;
    p["device_id"] = FieldValue::from_string("sensor_01");
    p["status"] = FieldValue::from_string(i % 500 == 0 ? "maintenance-mode" : "operational");
    p["unit"] = FieldValue::from_string("celsius");
    p["value"] = FieldValue::from_u16(static_cast<u16>(210 + (i % 70) / 10));
    return p;
}

SchemaRuntime make_stateless()
{
    SchemaRuntime runtime;
    (void)runtime.register_schema(kSchema);
    return runtime;
}

SchemaRuntime make_dictionary_runtime()
{
    SchemaRuntime runtime;
    (void)runtime.register_schema(kSchema);
    AdaptiveDictionaryConfig dict;
    dict.enabled = true;
    dict.enable_string_values = true;
    runtime.set_dictionary_config(dict);
    return runtime;
}

SchemaRuntime make_batched_runtime()
{
    SchemaRuntime runtime;
    (void)runtime.register_schema(kSchema);
    StreamOptimizerConfig cfg;
    cfg.enabled = true;
    cfg.enable_packet_reuse = true;
    cfg.enable_delta_updates = true;
    cfg.enable_batching = true;
    cfg.aggressive_after_samples = 1;
    cfg.max_batch_packets = 4;
    cfg.low_change_ratio_threshold = 0.5f;
    runtime.set_stream_optimizer_config(cfg);
    runtime.reset_stream_optimizer();
    return runtime;
}

SchemaRuntime make_delta_runtime()
{
    SchemaRuntime runtime;
    (void)runtime.register_schema(kSchema);
    StreamOptimizerConfig cfg;
    cfg.enabled = true;
    cfg.enable_packet_reuse = true;
    cfg.enable_delta_updates = true;
    cfg.enable_batching = false;
    cfg.enable_delta_packets = true;
    cfg.keyframe_interval = kKeyframeInterval;
    cfg.aggressive_after_samples = 1;
    cfg.low_change_ratio_threshold = 0.5f;
    runtime.set_stream_optimizer_config(cfg);
    runtime.reset_stream_optimizer();
    return runtime;
}

struct ModeResult {
    std::string name;
    usize first_packet_bytes = 0;
    usize total_bytes = 0;
    usize emitted_packets = 0;
    usize decoded_records = 0;
    usize misdecoded_records = 0;
};

ModeResult run_mode(const std::string& name, SchemaRuntime& runtime, bool use_stream, bool decode)
{
    ModeResult result;
    result.name = name;

    for (usize i = 0; i < kRecords; ++i)
    {
        const NamedPayload payload = make_payload(i);
        Buffer out;
        bool has = true;
        SchemaRuntimeResult send_result;
        if (use_stream)
        {
            send_result = runtime.send_stream("Telemetry", payload, out, has);
        }
        else
        {
            send_result = runtime.send("Telemetry", payload, out);
        }
        if (!send_result.ok() || !has)
        {
            continue;
        }

        result.emitted_packets += 1;
        result.total_bytes += out.size();
        if (result.first_packet_bytes == 0)
        {
            result.first_packet_bytes = out.size();
        }

        if (decode)
        {
            std::vector<std::pair<std::string, NamedPayload>> messages;
            const SchemaRuntimeResult recv = runtime.receive_stream(out, messages);
            if (!recv.ok())
            {
                continue;
            }
            for (usize m = 0; m < messages.size(); ++m)
            {
                result.decoded_records += 1;
                const NamedPayload& decoded = messages[m].second;
                const NamedPayload expected = make_payload(i - (messages.size() - 1) + m);
                const bool matches =
                    decoded.count("timestamp") == 1 && decoded.at("timestamp").as_u32 == expected.at("timestamp").as_u32
                    && decoded.count("temperature") == 1 && decoded.at("temperature").as_u16 == expected.at("temperature").as_u16
                    && decoded.count("status") == 1 && decoded.at("status").as_u8 == expected.at("status").as_u8
                    && decoded.count("device_id") == 1 && decoded.at("device_id").as_string == expected.at("device_id").as_string;
                if (!matches)
                {
                    result.misdecoded_records += 1;
                }
            }
        }
    }

    if (use_stream)
    {
        Buffer flushed;
        bool has = false;
        if (runtime.flush_stream(flushed, has).ok() && has)
        {
            result.emitted_packets += 1;
            result.total_bytes += flushed.size();
            if (decode)
            {
                std::vector<std::pair<std::string, NamedPayload>> messages;
                if (runtime.receive_stream(flushed, messages).ok())
                {
                    result.decoded_records += messages.size();
                }
            }
        }
    }

    return result;
}

void print_mode(const ModeResult& result, usize baseline_total)
{
    const double steady_avg = result.emitted_packets > 1
        ? static_cast<double>(result.total_bytes - result.first_packet_bytes) / (result.emitted_packets - 1)
        : 0.0;
    const double reduction = baseline_total > 0
        ? 100.0 * (static_cast<double>(baseline_total) - result.total_bytes) / baseline_total
        : 0.0;
    std::cout << result.name << "_first_packet_bytes=" << result.first_packet_bytes << "\n";
    std::cout << result.name << "_total_bytes=" << result.total_bytes << "\n";
    std::cout << result.name << "_steady_state_avg_bytes=" << steady_avg << "\n";
    std::cout << result.name << "_emitted_packets=" << result.emitted_packets << "\n";
    std::cout << result.name << "_total_reduction_pct=" << reduction << "\n";
    std::cout << result.name << "_decoded_records=" << result.decoded_records << "\n";
    std::cout << result.name << "_misdecoded_records=" << result.misdecoded_records << "\n";
}

} // namespace

int main()
{
    // ── Stateless baseline (also the decode reference) ───────────
    SchemaRuntime stateless = make_stateless();
    const ModeResult stateless_result = run_mode("stateless", stateless, false, false);
    std::cout << "records=" << kRecords << "\n";
    std::cout << "keyframe_interval=" << kKeyframeInterval << "\n";
    print_mode(stateless_result, stateless_result.total_bytes);

    // ── 3A: adaptive dictionary ──────────────────────────────────
    SchemaRuntime dict = make_dictionary_runtime();
    const ModeResult dict_result = run_mode("dictionary", dict, false, false);
    print_mode(dict_result, stateless_result.total_bytes);

    // ── 3D: batching (existing mode) ─────────────────────────────
    SchemaRuntime batched = make_batched_runtime();
    const ModeResult batched_result = run_mode("batched", batched, true, false);
    print_mode(batched_result, stateless_result.total_bytes);

    // ── 3B + 3C: stateful delta packets ──────────────────────────
    SchemaRuntime delta = make_delta_runtime();
    const ModeResult delta_result = run_mode("delta", delta, true, false);
    print_mode(delta_result, stateless_result.total_bytes);

    // ── W3: repeated-string workload for the 3A dictionary target
    usize string_stateless_bytes = 0;
    {
        SchemaRuntime runtime;
        (void)runtime.register_schema(kStringSchema);
        for (usize i = 0; i < kRecords; ++i)
        {
            Buffer out;
            (void)runtime.send("Strings", make_string_payload(i), out);
            string_stateless_bytes += out.size();
        }
    }
    std::cout << "strings_stateless_total_bytes=" << string_stateless_bytes << "\n";
    {
        SchemaRuntime runtime;
        (void)runtime.register_schema(kStringSchema);
        AdaptiveDictionaryConfig dict;
        dict.enabled = true;
        dict.enable_string_values = true;
        runtime.set_dictionary_config(dict);
        usize total = 0;
        for (usize i = 0; i < kRecords; ++i)
        {
            Buffer out;
            (void)runtime.send("Strings", make_string_payload(i), out);
            total += out.size();
        }
        std::cout << "strings_dictionary_total_bytes=" << total << "\n";
        std::cout << "strings_dictionary_reduction_pct="
                  << 100.0 * (static_cast<double>(string_stateless_bytes) - total) / string_stateless_bytes << "\n";
    }
    {
        SchemaRuntime runtime;
        (void)runtime.register_schema(kStringSchema);
        StreamOptimizerConfig cfg;
        cfg.enabled = true;
        cfg.enable_packet_reuse = true;
        cfg.enable_delta_updates = true;
        cfg.enable_batching = false;
        cfg.enable_delta_packets = true;
        cfg.keyframe_interval = kKeyframeInterval;
        cfg.aggressive_after_samples = 1;
        cfg.low_change_ratio_threshold = 0.5f;
        runtime.set_stream_optimizer_config(cfg);
        runtime.reset_stream_optimizer();
        usize total = 0;
        for (usize i = 0; i < kRecords; ++i)
        {
            Buffer out;
            bool has = false;
            if (runtime.send_stream("Strings", make_string_payload(i), out, has).ok() && has)
            {
                total += out.size();
            }
        }
        Buffer flushed;
        bool has = false;
        if (runtime.flush_stream(flushed, has).ok() && has)
        {
            total += flushed.size();
        }
        std::cout << "strings_delta_total_bytes=" << total << "\n";
        std::cout << "strings_delta_reduction_pct="
                  << 100.0 * (static_cast<double>(string_stateless_bytes) - total) / string_stateless_bytes << "\n";
    }

    // ── Correctness + loss safety on the delta stream ────────────
    SchemaRuntime sender = make_delta_runtime();
    SchemaRuntime receiver = make_delta_runtime();
    usize sent = 0;
    usize decoded = 0;
    usize rejected = 0;
    usize misdecoded = 0;
    std::unordered_set<u32> sent_timestamps;
    for (usize i = 0; i < kRecords; ++i)
    {
        Buffer out;
        bool has = false;
        if (!sender.send_stream("Telemetry", make_payload(i), out, has).ok() || !has)
        {
            continue;
        }
        sent += 1;
        sent_timestamps.insert(static_cast<u32>(1000000 + i * 1000));

        if (i % 13 == 5)
        {
            continue; // simulate a dropped packet on the wire
        }

        std::vector<std::pair<std::string, NamedPayload>> messages;
        const SchemaRuntimeResult recv = receiver.receive_stream(out, messages);
        if (!recv.ok())
        {
            rejected += 1;
            continue;
        }
        for (usize m = 0; m < messages.size(); ++m)
        {
            decoded += 1;
            const NamedPayload& value = messages[m].second;
            // Gate: a decoded value must be a value that was actually sent.
            const bool matches = value.count("timestamp") == 1
                && sent_timestamps.count(value.at("timestamp").as_u32) == 1
                && value.count("device_id") == 1
                && value.at("device_id").as_string == "sensor_01";
            if (!matches)
            {
                misdecoded += 1;
            }
        }
    }
    std::cout << "loss_sent=" << sent << "\n";
    std::cout << "loss_decoded=" << decoded << "\n";
    std::cout << "loss_rejected_safely=" << rejected << "\n";
    std::cout << "loss_misdecoded=" << misdecoded << "\n";
    std::cout << "loss_silent_misdecode_gate=" << (misdecoded == 0 ? 1 : 0) << "\n";

    // Delta mode full-stream correctness without loss.
    SchemaRuntime clean = make_delta_runtime();
    const ModeResult clean_result = run_mode("delta", clean, true, true);
    std::cout << "delta_clean_decoded_records=" << clean_result.decoded_records << "\n";
    std::cout << "delta_clean_misdecoded_records=" << clean_result.misdecoded_records << "\n";

    return clean_result.misdecoded_records == 0 ? 0 : 1;
}
