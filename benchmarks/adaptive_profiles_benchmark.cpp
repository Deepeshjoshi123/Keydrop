// Phase 4 adaptive profiles benchmark.
//
// Compares fixed (stateless) mode with the adaptive profile mode on two
// streams:
//   suitable   — constant strings, slowly changing values: the profiler
//                should enable dictionary + delta packets and lower
//                amortized bytes (target 20-60%).
//   unsuitable — unique strings, every field changing: the profiler should
//                keep dictionary + deltas disabled so the packet-size
//                overhead stays <= 5%.
//
// Also reports the number of evaluation windows with an incorrect decision
// and verifies lossless reconstruction of every emitted packet.
//
// Output is key=value lines suitable for CSV capture.

#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "keydrop/schema/profiles.hpp"
#include "keydrop/schema/schema_runtime.hpp"

using namespace keydrop;

namespace {

constexpr usize kRecords = 10000;
constexpr usize kWindowSize = 100;

const SchemaDef kSchema {
    "Telemetry",
    71,
    {
        FieldDef {"device_id", FieldType::string, 0, FieldConstraints {true, 32}},
        FieldDef {"status", FieldType::string, 1, FieldConstraints {true, 32}},
        FieldDef {"timestamp", FieldType::u32, 2, {}},
        FieldDef {"temperature", FieldType::u16, 3, {}},
    }
};

NamedPayload suitable(usize i)
{
    NamedPayload p;
    p["device_id"] = FieldValue::from_string("sensor_01");
    p["status"] = FieldValue::from_string("operational");
    p["timestamp"] = FieldValue::from_u32(static_cast<u32>(1000000 + i * 1000));
    p["temperature"] = FieldValue::from_u16(static_cast<u16>(210 + (i % 70) / 10));
    return p;
}

NamedPayload unsuitable(usize i)
{
    NamedPayload p;
    p["device_id"] = FieldValue::from_string("device-" + std::to_string(i) + "-unique");
    p["status"] = FieldValue::from_string("state-" + std::to_string(i % 7));
    p["timestamp"] = FieldValue::from_u32(static_cast<u32>(i * 997 + 13));
    p["temperature"] = FieldValue::from_u16(static_cast<u16>(i * 31 % 60000));
    return p;
}

SchemaRuntime make_fixed()
{
    SchemaRuntime runtime;
    (void)runtime.register_schema(kSchema);
    return runtime;
}

SchemaRuntime make_adaptive()
{
    SchemaRuntime runtime;
    (void)runtime.register_schema(kSchema);
    ProfileSettings profile;
    (void)try_get_profile("telemetry-balanced", profile);
    apply_profile(runtime, profile);
    AdaptiveProfilerConfig adaptive;
    adaptive.enabled = true;
    adaptive.window_size = kWindowSize;
    runtime.set_adaptive_config(adaptive);
    runtime.reset_adaptive_profiler();
    return runtime;
}

struct RunResult {
    usize total_bytes = 0;
    usize packets = 0;
    usize decoded = 0;
    usize misdecoded = 0;
    usize wrong_dict_windows = 0;
    usize wrong_delta_windows = 0;
};

RunResult run_fixed(NamedPayload (*payload_fn)(usize))
{
    SchemaRuntime runtime = make_fixed();
    RunResult result;
    for (usize i = 0; i < kRecords; ++i)
    {
        Buffer out;
        if (!runtime.send("Telemetry", payload_fn(i), out).ok())
        {
            continue;
        }
        result.total_bytes += out.size();
        result.packets += 1;
    }
    return result;
}

RunResult run_adaptive(NamedPayload (*payload_fn)(usize), bool expect_dict, bool expect_delta)
{
    SchemaRuntime runtime = make_adaptive();
    RunResult result;
    // Lossless verification is order-independent (batches and deltas can
    // decode out of emission order): count sent and decoded timestamps as
    // multisets and compare them after the run.
    std::unordered_map<u32, usize> sent_count;
    std::unordered_map<u32, usize> decoded_count;
    for (usize i = 0; i < kRecords; ++i)
    {
        sent_count[payload_fn(i)["timestamp"].as_u32] += 1;

        Buffer out;
        bool has = false;
        if (!runtime.send_stream("Telemetry", payload_fn(i), out, has).ok() || !has)
        {
            continue;
        }
        result.total_bytes += out.size();
        result.packets += 1;

        std::vector<std::pair<std::string, NamedPayload>> messages;
        if (!runtime.receive_stream(out, messages).ok())
        {
            continue;
        }
        for (usize m = 0; m < messages.size(); ++m)
        {
            const NamedPayload& value = messages[m].second;
            result.decoded += 1;
            if (value.count("timestamp") == 1)
            {
                decoded_count[value.at("timestamp").as_u32] += 1;
            }
        }

        // Decision check at each window boundary (after the apply point).
        if (i % kWindowSize == kWindowSize - 1)
        {
            const bool dict_on = runtime.dictionary_config().enabled;
            const bool delta_on = runtime.stream_optimizer_config().enable_delta_packets;
            if (dict_on != expect_dict)
            {
                result.wrong_dict_windows += 1;
            }
            if (delta_on != expect_delta)
            {
                result.wrong_delta_windows += 1;
            }
        }
    }

    Buffer flushed;
    bool has = false;
    if (runtime.flush_stream(flushed, has).ok() && has)
    {
        result.total_bytes += flushed.size();
        result.packets += 1;
        std::vector<std::pair<std::string, NamedPayload>> messages;
        if (runtime.receive_stream(flushed, messages).ok())
        {
            for (usize m = 0; m < messages.size(); ++m)
            {
                const NamedPayload& value = messages[m].second;
                result.decoded += 1;
                if (value.count("timestamp") == 1)
                {
                    decoded_count[value.at("timestamp").as_u32] += 1;
                }
            }
        }
    }

    // Symmetric multiset difference: a decode is wrong if it over- or
    // under-counts any sent timestamp.
    usize symmetric_difference = 0;
    for (std::unordered_map<u32, usize>::const_iterator it = decoded_count.begin(); it != decoded_count.end(); ++it)
    {
        const std::unordered_map<u32, usize>::const_iterator sent_it = sent_count.find(it->first);
        const usize sent = sent_it == sent_count.end() ? 0 : sent_it->second;
        symmetric_difference += it->second > sent ? it->second - sent : sent - it->second;
    }
    for (std::unordered_map<u32, usize>::const_iterator it = sent_count.begin(); it != sent_count.end(); ++it)
    {
        if (decoded_count.find(it->first) == decoded_count.end())
        {
            symmetric_difference += it->second;
        }
    }
    result.misdecoded = symmetric_difference;
    return result;
}

} // namespace

int main()
{
    std::cout << "records=" << kRecords << "\n";
    std::cout << "window_size=" << kWindowSize << "\n";

    const RunResult suitable_fixed = run_fixed(suitable);
    std::cout << "suitable_fixed_total_bytes=" << suitable_fixed.total_bytes << "\n";

    const RunResult suitable_adaptive = run_adaptive(suitable, true, true);
    std::cout << "suitable_adaptive_total_bytes=" << suitable_adaptive.total_bytes << "\n";
    std::cout << "suitable_adaptive_reduction_pct="
              << 100.0 * (static_cast<double>(suitable_fixed.total_bytes) - suitable_adaptive.total_bytes) / suitable_fixed.total_bytes << "\n";
    std::cout << "suitable_decoded=" << suitable_adaptive.decoded << "\n";
    std::cout << "suitable_misdecoded=" << suitable_adaptive.misdecoded << "\n";
    std::cout << "suitable_wrong_dict_windows=" << suitable_adaptive.wrong_dict_windows << "\n";
    std::cout << "suitable_wrong_delta_windows=" << suitable_adaptive.wrong_delta_windows << "\n";

    const RunResult unsuitable_fixed = run_fixed(unsuitable);
    std::cout << "unsuitable_fixed_total_bytes=" << unsuitable_fixed.total_bytes << "\n";

    const RunResult unsuitable_adaptive = run_adaptive(unsuitable, false, false);
    std::cout << "unsuitable_adaptive_total_bytes=" << unsuitable_adaptive.total_bytes << "\n";
    std::cout << "unsuitable_adaptive_overhead_pct="
              << 100.0 * (static_cast<double>(unsuitable_adaptive.total_bytes) - unsuitable_fixed.total_bytes) / unsuitable_fixed.total_bytes << "\n";
    std::cout << "unsuitable_decoded=" << unsuitable_adaptive.decoded << "\n";
    std::cout << "unsuitable_misdecoded=" << unsuitable_adaptive.misdecoded << "\n";
    std::cout << "unsuitable_wrong_dict_windows=" << unsuitable_adaptive.wrong_dict_windows << "\n";
    std::cout << "unsuitable_wrong_delta_windows=" << unsuitable_adaptive.wrong_delta_windows << "\n";

    const bool pass = suitable_adaptive.misdecoded == 0 && unsuitable_adaptive.misdecoded == 0;
    return pass ? 0 : 1;
}
