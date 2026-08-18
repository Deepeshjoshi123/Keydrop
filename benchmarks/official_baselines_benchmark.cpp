// Phase 7 official comparative benchmark.
//
// Compares Keydrop against OFFICIAL external implementations, clearly
// labeled per row:
//
//   json_nlohmann_<version>          — nlohmann::json (official, named)
//   protobuf_<version>               — protoc-generated C++ code
//   msgpack_official                 — msgpack-c (when installed)
//   keydrop_stateless                — Keydrop send_ordered (in-repo)
//   keydrop_stateful                 — Keydrop delta stream (in-repo,
//                                      steady-state bytes reported
//                                      separately from the first packet)
//
// Workloads (explicit, reproducible definitions):
//   W1 fixed record   {temperature:32, humidity:70, device_id:"sensor_01"}
//   W3 string-heavy   {device_id:"sensor_01", status:"operational",
//                      unit:"celsius", value:210}
//   W4 timestamp      {timestamp:1000000+i*1000, temperature:210+(i%7)}
//
// Every row reports: workload, packet/steady-state bytes, encode latency,
// decode latency, throughput, and encode-window allocation counts
// (HeapTracker gross allocation counting — NOT total process memory;
// see the benchmark terminology in the Phase 0 docs).
//
// Formats whose libraries are not installed print a single
// "<format>_unavailable=1" line instead of fabricated rows.
//
// Output is key=value lines suitable for CSV capture.

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "keydrop/benchmark/heap_tracker.hpp"
#include "keydrop/schema/schema_runtime.hpp"

#ifdef KEYDROP_HAS_PROTOBUF
#include "telemetry.pb.h"
#endif

#ifdef KEYDROP_HAS_JSON
#include <nlohmann/json.hpp>
#endif

#ifdef KEYDROP_HAS_MSGPACK
#include <msgpack.hpp>
#endif

using namespace keydrop;

namespace {

usize parse_iterations(int argc, char** argv)
{
    if (argc < 2)
    {
        return 10000;
    }
    const long parsed = std::strtol(argv[1], nullptr, 10);
    return parsed > 0 ? static_cast<usize>(parsed) : 10000;
}

struct Row {
    std::string format;
    std::string workload;
    usize bytes = 0;
    double encode_ns = 0.0;
    double decode_ns = 0.0;
    double throughput_per_sec = 0.0;
    double allocations = 0.0;
    double allocated_bytes = 0.0;
};

void print_row(const Row& row)
{
    std::cout << "format=" << row.format << "\n";
    std::cout << "workload=" << row.workload << "\n";
    std::cout << "packet_bytes=" << row.bytes << "\n";
    std::cout << "avg_encode_ns=" << row.encode_ns << "\n";
    std::cout << "avg_decode_ns=" << row.decode_ns << "\n";
    std::cout << "throughput_per_sec=" << row.throughput_per_sec << "\n";
    std::cout << "allocations_per_encode=" << row.allocations << "\n";
    std::cout << "allocated_bytes_per_encode=" << row.allocated_bytes << "\n";
}

Row measure(
    const std::string& format,
    const std::string& workload,
    usize iterations,
    void (*encode)(std::string& out),
    bool (*decode)(const std::string& in)
)
{
    Row row;
    row.format = format;
    row.workload = workload;

    // Warm-up (discarded) + capture encoded size.
    std::string encoded;
    encode(encoded);
    row.bytes = encoded.size();

    usize allocations = 0;
    usize allocated_bytes = 0;
    u64 encode_ns = 0;
    {
        HeapTracker::reset();
        HeapTracker::begin();
        const auto start = std::chrono::steady_clock::now();
        for (usize i = 0; i < iterations; ++i)
        {
            std::string out;
            encode(out);
        }
        encode_ns = static_cast<u64>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start).count());
        HeapTracker::end();
        allocations = HeapTracker::allocation_count();
        allocated_bytes = HeapTracker::allocated_bytes();
    }

    u64 decode_ns = 0;
    {
        const auto start = std::chrono::steady_clock::now();
        for (usize i = 0; i < iterations; ++i)
        {
            if (!decode(encoded))
            {
                row.encode_ns = -1.0; // round-trip failure marker
                return row;
            }
        }
        decode_ns = static_cast<u64>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start).count());
    }

    row.encode_ns = static_cast<double>(encode_ns) / iterations;
    row.decode_ns = static_cast<double>(decode_ns) / iterations;
    row.throughput_per_sec = iterations / ((encode_ns + decode_ns) / 1000000000.0);
    row.allocations = static_cast<double>(allocations) / iterations;
    row.allocated_bytes = static_cast<double>(allocated_bytes) / iterations;
    return row;
}

// ── Keydrop rows ─────────────────────────────────────────────────

const SchemaDef kFixedSchema {
    "BenchmarkPayload",
    91,
    {
        FieldDef {"temperature", FieldType::u16, 0, {}},
        FieldDef {"humidity", FieldType::u16, 1, {}},
        FieldDef {"device_id", FieldType::string, 2, FieldConstraints {true, 64}},
    }
};

const SchemaDef kStringSchema {
    "Strings",
    92,
    {
        FieldDef {"device_id", FieldType::string, 0, FieldConstraints {true, 32}},
        FieldDef {"status", FieldType::string, 1, FieldConstraints {true, 32}},
        FieldDef {"unit", FieldType::string, 2, FieldConstraints {true, 8}},
        FieldDef {"value", FieldType::u16, 3, {}},
    }
};

const SchemaDef kTimeSchema {
    "Time",
    93,
    {
        FieldDef {"timestamp", FieldType::u32, 0, {}},
        FieldDef {"device_id", FieldType::string, 1, FieldConstraints {true, 32}},
        FieldDef {"temperature", FieldType::u16, 2, {}},
        FieldDef {"humidity", FieldType::u16, 3, {}},
        FieldDef {"status", FieldType::u8, 4, {}},
    }
};

NamedPayload fixed_payload()
{
    NamedPayload p;
    p["temperature"] = FieldValue::from_u16(32);
    p["humidity"] = FieldValue::from_u16(70);
    p["device_id"] = FieldValue::from_string("sensor_01");
    return p;
}

NamedPayload string_payload()
{
    NamedPayload p;
    p["device_id"] = FieldValue::from_string("sensor_01");
    p["status"] = FieldValue::from_string("operational");
    p["unit"] = FieldValue::from_string("celsius");
    p["value"] = FieldValue::from_u16(210);
    return p;
}

NamedPayload time_payload(usize i)
{
    NamedPayload p;
    p["timestamp"] = FieldValue::from_u32(static_cast<u32>(1000000 + i * 1000));
    p["device_id"] = FieldValue::from_string("sensor_01");
    p["temperature"] = FieldValue::from_u16(static_cast<u16>(210 + (i % 70) / 10));
    p["humidity"] = FieldValue::from_u16(static_cast<u16>(550 + (i % 250) / 25));
    p["status"] = FieldValue::from_u8(i % 500 == 0 ? 1 : 0);
    return p;
}

SchemaRuntime make_runtime(const SchemaDef& schema)
{
    SchemaRuntime runtime;
    (void)runtime.register_schema(schema);
    return runtime;
}

OrderedPayload ordered_from(const SchemaDef& schema, const NamedPayload& named);

Row keydrop_stateless_row(
    const std::string& workload,
    const SchemaDef& schema,
    const NamedPayload& payload,
    usize iterations
)
{
    static SchemaRuntime runtime = make_runtime(kFixedSchema);
    static SchemaRuntime string_runtime = make_runtime(kStringSchema);
    static SchemaRuntime time_runtime = make_runtime(kTimeSchema);
    SchemaRuntime* selected = &runtime;
    const std::string schema_name = schema.schema_name;
    if (schema_name == "Strings")
    {
        selected = &string_runtime;
    }
    else if (schema_name == "Time")
    {
        selected = &time_runtime;
    }

    std::string encoded;
    Buffer packet;
    (void)selected->send_ordered(schema_name, ordered_from(schema, payload), packet);
    encoded.assign(reinterpret_cast<const char*>(packet.data().data()), packet.size());

    Row row;
    row.format = "keydrop_stateless";
    row.workload = workload;
    row.bytes = encoded.size();

    usize allocations = 0;
    usize allocated_bytes = 0;
    u64 encode_ns = 0;
    {
        HeapTracker::reset();
        HeapTracker::begin();
        const auto start = std::chrono::steady_clock::now();
        for (usize i = 0; i < iterations; ++i)
        {
            Buffer out;
            (void)selected->send_ordered(schema_name, ordered_from(schema, payload), out);
        }
        encode_ns = static_cast<u64>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start).count());
        HeapTracker::end();
        allocations = HeapTracker::allocation_count();
        allocated_bytes = HeapTracker::allocated_bytes();
    }

    u64 decode_ns = 0;
    {
        const auto start = std::chrono::steady_clock::now();
        for (usize i = 0; i < iterations; ++i)
        {
            OrderedPayload decoded;
            std::string schema_name_out;
            if (!selected->receive_ordered(packet, schema_name_out, decoded).ok())
            {
                row.encode_ns = -1.0;
                return row;
            }
        }
        decode_ns = static_cast<u64>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start).count());
    }

    row.encode_ns = static_cast<double>(encode_ns) / iterations;
    row.decode_ns = static_cast<double>(decode_ns) / iterations;
    row.throughput_per_sec = iterations / ((encode_ns + decode_ns) / 1000000000.0);
    row.allocations = static_cast<double>(allocations) / iterations;
    row.allocated_bytes = static_cast<double>(allocated_bytes) / iterations;
    return row;
}

OrderedPayload ordered_from(const SchemaDef& schema, const NamedPayload& named)
{
    OrderedPayload ordered;
    ordered.reserve(schema.fields.size());
    for (usize i = 0; i < schema.fields.size(); ++i)
    {
        const NamedPayload::const_iterator it = named.find(schema.fields[i].name);
        if (it != named.end())
        {
            ordered.push_back(it->second);
        }
    }
    return ordered;
}

void keydrop_stateful_rows(const std::string& workload, usize iterations)
{
    constexpr usize kRecords = 10000;
    constexpr usize kKeyframeInterval = 100;

    SchemaRuntime runtime = make_runtime(kTimeSchema);
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

    usize total_bytes = 0;
    usize first_packet_bytes = 0;
    usize emitted = 0;
    u64 encode_ns = 0;
    u64 decode_ns = 0;
    usize failures = 0;

    {
        const auto encode_start = std::chrono::steady_clock::now();
        for (usize i = 0; i < kRecords; ++i)
        {
            Buffer out;
            bool has = false;
            if (!runtime.send_stream("Time", time_payload(i), out, has).ok())
            {
                failures += 1;
                continue;
            }
            if (!has)
            {
                continue;
            }
            emitted += 1;
            total_bytes += out.size();
            if (first_packet_bytes == 0)
            {
                first_packet_bytes = out.size();
            }

            std::vector<std::pair<std::string, NamedPayload>> messages;
            if (!runtime.receive_stream(out, messages).ok())
            {
                failures += 1;
            }
        }
        encode_ns = static_cast<u64>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - encode_start).count());
    }
    (void)decode_ns;
    (void)iterations;

    const double steady_avg = emitted > 1
        ? static_cast<double>(total_bytes - first_packet_bytes) / (emitted - 1)
        : 0.0;

    std::cout << "format=keydrop_stateful\n";
    std::cout << "workload=" << workload << "\n";
    std::cout << "records=" << kRecords << "\n";
    std::cout << "keyframe_interval=" << kKeyframeInterval << "\n";
    std::cout << "first_packet_bytes=" << first_packet_bytes << "\n";
    std::cout << "steady_state_avg_bytes=" << steady_avg << "\n";
    std::cout << "total_stream_bytes=" << total_bytes << "\n";
    std::cout << "avg_encode_ns=" << static_cast<double>(encode_ns) / kRecords << "\n";
    std::cout << "round_trip_failures=" << failures << "\n";
}

// ── External format encode/decode ────────────────────────────────

#ifdef KEYDROP_HAS_JSON
void json_encode_fixed(std::string& out)
{
    nlohmann::json j = {
        {"temperature", 32},
        {"humidity", 70},
        {"device_id", "sensor_01"},
    };
    out = j.dump();
}

bool json_decode_fixed(const std::string& in)
{
    const nlohmann::json j = nlohmann::json::parse(in);
    return j["temperature"].get<unsigned>() == 32
        && j["device_id"].get<std::string>() == "sensor_01";
}

void json_encode_strings(std::string& out)
{
    nlohmann::json j = {
        {"device_id", "sensor_01"},
        {"status", "operational"},
        {"unit", "celsius"},
        {"value", 210},
    };
    out = j.dump();
}

bool json_decode_strings(const std::string& in)
{
    const nlohmann::json j = nlohmann::json::parse(in);
    return j["device_id"].get<std::string>() == "sensor_01"
        && j["status"].get<std::string>() == "operational";
}
#endif

#ifdef KEYDROP_HAS_PROTOBUF
void protobuf_encode_fixed(std::string& out)
{
    keydrop::bench::FixedRecord record;
    record.set_temperature(32);
    record.set_humidity(70);
    record.set_device_id("sensor_01");
    record.SerializeToString(&out);
}

bool protobuf_decode_fixed(const std::string& in)
{
    keydrop::bench::FixedRecord record;
    return record.ParseFromString(in)
        && record.temperature() == 32
        && record.device_id() == "sensor_01";
}

void protobuf_encode_strings(std::string& out)
{
    keydrop::bench::StringRecord record;
    record.set_device_id("sensor_01");
    record.set_status("operational");
    record.set_unit("celsius");
    record.set_value(210);
    record.SerializeToString(&out);
}

bool protobuf_decode_strings(const std::string& in)
{
    keydrop::bench::StringRecord record;
    return record.ParseFromString(in)
        && record.device_id() == "sensor_01"
        && record.status() == "operational";
}
#endif

#ifdef KEYDROP_HAS_MSGPACK
void msgpack_encode_fixed(std::string& out)
{
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, std::make_tuple(32u, 70u, std::string("sensor_01")));
    out.assign(buffer.data(), buffer.size());
}

bool msgpack_decode_fixed(const std::string& in)
{
    const msgpack::object_handle handle = msgpack::unpack(in.data(), in.size());
    const auto tuple = handle.get().as<std::tuple<unsigned, unsigned, std::string>>();
    return std::get<0>(tuple) == 32 && std::get<2>(tuple) == "sensor_01";
}

void msgpack_encode_strings(std::string& out)
{
    msgpack::sbuffer buffer;
    msgpack::pack(buffer, std::make_tuple(
        std::string("sensor_01"), std::string("operational"),
        std::string("celsius"), 210u));
    out.assign(buffer.data(), buffer.size());
}

bool msgpack_decode_strings(const std::string& in)
{
    const msgpack::object_handle handle = msgpack::unpack(in.data(), in.size());
    const auto tuple = handle.get().as<std::tuple<std::string, std::string, std::string, unsigned>>();
    return std::get<0>(tuple) == "sensor_01" && std::get<1>(tuple) == "operational";
}
#endif

} // namespace

int main(int argc, char** argv)
{
    const usize iterations = parse_iterations(argc, argv);
    std::cout << "classification=official_external_baselines\n";
    std::cout << "iterations=" << iterations << "\n";

#ifdef KEYDROP_HAS_JSON
    std::cout << "json_library=nlohmann_json " << NLOHMANN_JSON_VERSION_MAJOR
              << "." << NLOHMANN_JSON_VERSION_MINOR << "."
              << NLOHMANN_JSON_VERSION_PATCH << "\n";
#else
    std::cout << "json_library=nlohmann_json unavailable\n";
    std::cout << "json_nlohmann_unavailable=1\n";
#endif

#ifdef KEYDROP_HAS_PROTOBUF
    std::cout << "protobuf_library=" << GOOGLE_PROTOBUF_VERSION << "\n";
#else
    std::cout << "protobuf_library=unavailable\n";
    std::cout << "protobuf_unavailable=1\n";
#endif

#ifdef KEYDROP_HAS_MSGPACK
    std::cout << "msgpack_library=msgpack-c " << MSGPACK_VERSION_MAJOR
              << "." << MSGPACK_VERSION_MINOR << "." << MSGPACK_VERSION_REVISION << "\n";
#else
    std::cout << "msgpack_library=unavailable\n";
    std::cout << "msgpack_unavailable=1\n";
#endif

    // ── W1: fixed record ─────────────────────────────────────────
    print_row(keydrop_stateless_row("W1_fixed_record", kFixedSchema, fixed_payload(), iterations));

#ifdef KEYDROP_HAS_JSON
    print_row(measure("json_nlohmann", "W1_fixed_record", iterations, json_encode_fixed, json_decode_fixed));
#endif

#ifdef KEYDROP_HAS_PROTOBUF
    print_row(measure("protobuf", "W1_fixed_record", iterations, protobuf_encode_fixed, protobuf_decode_fixed));
#endif

#ifdef KEYDROP_HAS_MSGPACK
    print_row(measure("msgpack_official", "W1_fixed_record", iterations, msgpack_encode_fixed, msgpack_decode_fixed));
#endif

    // ── W3: string-heavy ─────────────────────────────────────────
    print_row(keydrop_stateless_row("W3_string_heavy", kStringSchema, string_payload(), iterations));

#ifdef KEYDROP_HAS_JSON
    print_row(measure("json_nlohmann", "W3_string_heavy", iterations, json_encode_strings, json_decode_strings));
#endif

#ifdef KEYDROP_HAS_PROTOBUF
    print_row(measure("protobuf", "W3_string_heavy", iterations, protobuf_encode_strings, protobuf_decode_strings));
#endif

#ifdef KEYDROP_HAS_MSGPACK
    print_row(measure("msgpack_official", "W3_string_heavy", iterations, msgpack_encode_strings, msgpack_decode_strings));
#endif

    // ── W4: periodic timestamp stream (stateful vs stateless) ────
    print_row(keydrop_stateless_row("W4_timestamp_stream", kTimeSchema, time_payload(0), iterations));
    keydrop_stateful_rows("W4_timestamp_stream", iterations);

    return 0;
}
