// Phase 2 stateless fast-path benchmark.
//
// Compares the general SchemaRuntime path (send_ordered/receive_ordered —
// the fixed-record baseline used by benchmark_runner) with the FastCodec
// path (fast_encode/fast_decode). The baseline loop constructs its
// OrderedPayload per iteration exactly like the existing benchmark runner,
// schema-name literal included. The fast loop models the steady-state
// caller pattern the fast path targets: a caller-owned FieldValue array
// with in-place numeric updates, a caller-owned output buffer whose
// capacity is reused across calls, and a pre-built schema-name string.
// Allocation counts cover the encode window only (HeapTracker), matching
// the existing repository benchmark terminology.
//
// Output is key=value lines suitable for CSV capture.

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

#include "keydrop/benchmark/heap_tracker.hpp"
#include "keydrop/schema/schema_runtime.hpp"

using namespace keydrop;

namespace {

constexpr u16 kMessageId = 91;

usize parse_iterations(int argc, char** argv)
{
    if (argc < 2)
    {
        return 100000;
    }

    const long parsed = std::strtol(argv[1], nullptr, 10);
    return parsed > 0 ? static_cast<usize>(parsed) : 100000;
}

SchemaRuntime make_runtime()
{
    SchemaRuntime runtime;
    const SchemaDef schema {
        "BenchmarkPayload",
        kMessageId,
        {
            FieldDef {"temperature", FieldType::u16, 0, {}},
            FieldDef {"humidity", FieldType::u16, 1, {}},
            FieldDef {"device_id", FieldType::string, 2, FieldConstraints {true, 64}},
        }
    };
    (void)runtime.register_schema(schema);
    return runtime;
}

double reduction_pct(double baseline, double fast)
{
    if (baseline <= 0.0)
    {
        return 0.0;
    }

    return 100.0 * (baseline - fast) / baseline;
}

std::string decoded_device_id(const FastDecodedField& field)
{
    if (field.owned_string)
    {
        return field.owned;
    }

    return std::string(
        reinterpret_cast<const char*>(field.view.bytes()),
        field.view.size()
    );
}

} // namespace

int main(int argc, char** argv)
{
    const usize iterations = parse_iterations(argc, argv);
    SchemaRuntime runtime = make_runtime();
    // Steady-state caller pattern for the fast path: the schema-name string
    // is built once (16 characters — a literal would re-allocate per call).
    const std::string fast_schema_name = "BenchmarkPayload";

    // ── Correctness: byte equality + repeated round-trips ────────
    FieldValue fast_values[3];
    fast_values[0] = FieldValue::from_u16(32);
    fast_values[1] = FieldValue::from_u16(70);
    fast_values[2] = FieldValue::from_string("sensor_01");

    OrderedPayload baseline_payload;
    baseline_payload.reserve(3);
    baseline_payload.push_back(fast_values[0]);
    baseline_payload.push_back(fast_values[1]);
    baseline_payload.push_back(fast_values[2]);

    Buffer fast_packet;
    Buffer baseline_packet;
    if (!runtime.fast_encode("BenchmarkPayload", fast_values, 3, fast_packet).ok()
        || !runtime.send_ordered("BenchmarkPayload", baseline_payload, baseline_packet).ok()
        || fast_packet.data() != baseline_packet.data())
    {
        std::cerr << "packet_size_identical=0\n";
        return 1;
    }

    std::string schema_name;
    FastDecodedField fast_fields[3];
    usize field_count = 0;
    usize round_trip_failures = 0;
    for (usize i = 0; i < iterations; ++i)
    {
        fast_values[0] = FieldValue::from_u16(static_cast<u16>(32 + (i % 100)));
        fast_values[1] = FieldValue::from_u16(static_cast<u16>(70 + (i % 50)));
        const bool ok =
            runtime.fast_encode(fast_schema_name, fast_values, 3, fast_packet).ok()
            && runtime.fast_decode(fast_packet, schema_name, fast_fields, 3, field_count).ok()
            && field_count == 3
            && fast_fields[0].as_integer == fast_values[0].as_u16
            && fast_fields[1].as_integer == fast_values[1].as_u16
            && decoded_device_id(fast_fields[2]) == "sensor_01";
        if (!ok)
        {
            ++round_trip_failures;
        }
    }
    fast_values[0] = FieldValue::from_u16(32);
    fast_values[1] = FieldValue::from_u16(70);
    (void)runtime.fast_encode(fast_schema_name, fast_values, 3, fast_packet);

    // ── Warm-up (discarded) ──────────────────────────────────────
    const usize warm_up = iterations / 10;
    for (usize i = 0; i < warm_up; ++i)
    {
        OrderedPayload payload;
        payload.reserve(3);
        payload.push_back(FieldValue::from_u16(32));
        payload.push_back(FieldValue::from_u16(70));
        payload.push_back(FieldValue::from_string("sensor_01"));
        Buffer out;
        (void)runtime.send_ordered("BenchmarkPayload", payload, out);
        (void)runtime.fast_encode(fast_schema_name, fast_values, 3, out);
        (void)runtime.fast_decode(fast_packet, schema_name, fast_fields, 3, field_count);
    }

    // ── Baseline encode (general path, fresh payload per iteration)
    usize baseline_allocations = 0;
    usize baseline_allocated_bytes = 0;
    u64 baseline_encode_ns = 0;
    {
        HeapTracker::reset();
        HeapTracker::begin();
        const auto start = std::chrono::steady_clock::now();
        for (usize i = 0; i < iterations; ++i)
        {
            OrderedPayload payload;
            payload.reserve(3);
            payload.push_back(FieldValue::from_u16(32));
            payload.push_back(FieldValue::from_u16(70));
            payload.push_back(FieldValue::from_string("sensor_01"));
            Buffer out;
            if (!runtime.send_ordered("BenchmarkPayload", payload, out).ok())
            {
                return 1;
            }
        }
        baseline_encode_ns = static_cast<u64>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start).count());
        HeapTracker::end();
        baseline_allocations = HeapTracker::allocation_count();
        baseline_allocated_bytes = HeapTracker::allocated_bytes();
    }

    // ── Fast encode (steady-state: reused array + reused buffer) ─
    usize fast_allocations = 0;
    usize fast_allocated_bytes = 0;
    u64 fast_encode_ns = 0;
    {
        Buffer reused;
        reused.reserve(64);
        HeapTracker::reset();
        HeapTracker::begin();
        const auto start = std::chrono::steady_clock::now();
        for (usize i = 0; i < iterations; ++i)
        {
            fast_values[0] = FieldValue::from_u16(static_cast<u16>(32 + (i % 100)));
            fast_values[1] = FieldValue::from_u16(static_cast<u16>(70 + (i % 50)));
            if (!runtime.fast_encode(fast_schema_name, fast_values, 3, reused).ok())
            {
                return 1;
            }
        }
        fast_encode_ns = static_cast<u64>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start).count());
        HeapTracker::end();
        fast_allocations = HeapTracker::allocation_count();
        fast_allocated_bytes = HeapTracker::allocated_bytes();
    }

    // ── Baseline decode (general path) ───────────────────────────
    u64 baseline_decode_ns = 0;
    {
        OrderedPayload ordered;
        ordered.reserve(3);
        const auto start = std::chrono::steady_clock::now();
        for (usize i = 0; i < iterations; ++i)
        {
            ordered.clear();
            if (!runtime.receive_ordered(baseline_packet, schema_name, ordered).ok())
            {
                return 1;
            }
        }
        baseline_decode_ns = static_cast<u64>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start).count());
    }

    // ── Fast decode (zero-copy views) ────────────────────────────
    u64 fast_decode_ns = 0;
    {
        const auto start = std::chrono::steady_clock::now();
        for (usize i = 0; i < iterations; ++i)
        {
            if (!runtime.fast_decode(fast_packet, schema_name, fast_fields, 3, field_count).ok())
            {
                return 1;
            }
        }
        fast_decode_ns = static_cast<u64>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - start).count());
    }

    // ── Report ───────────────────────────────────────────────────
    const double baseline_encode = static_cast<double>(baseline_encode_ns) / iterations;
    const double fast_encode = static_cast<double>(fast_encode_ns) / iterations;
    const double baseline_decode = static_cast<double>(baseline_decode_ns) / iterations;
    const double fast_decode = static_cast<double>(fast_decode_ns) / iterations;
    const double baseline_throughput = iterations / ((baseline_encode_ns + baseline_decode_ns) / 1000000000.0);
    const double fast_throughput = iterations / ((fast_encode_ns + fast_decode_ns) / 1000000000.0);

    std::cout << "iterations=" << iterations << "\n";
    std::cout << "packet_size_bytes=" << fast_packet.size() << "\n";
    std::cout << "packet_size_identical=1\n";
    std::cout << "round_trip_failures=" << round_trip_failures << "\n";
    std::cout << "baseline_avg_encode_ns=" << baseline_encode << "\n";
    std::cout << "fast_avg_encode_ns=" << fast_encode << "\n";
    std::cout << "baseline_avg_decode_ns=" << baseline_decode << "\n";
    std::cout << "fast_avg_decode_ns=" << fast_decode << "\n";
    std::cout << "baseline_throughput_per_sec=" << baseline_throughput << "\n";
    std::cout << "fast_throughput_per_sec=" << fast_throughput << "\n";
    std::cout << "baseline_allocations_per_encode=" << static_cast<double>(baseline_allocations) / iterations << "\n";
    std::cout << "fast_allocations_per_encode=" << static_cast<double>(fast_allocations) / iterations << "\n";
    std::cout << "baseline_allocated_bytes_per_encode=" << static_cast<double>(baseline_allocated_bytes) / iterations << "\n";
    std::cout << "fast_allocated_bytes_per_encode=" << static_cast<double>(fast_allocated_bytes) / iterations << "\n";
    std::cout << "encode_latency_reduction_pct=" << reduction_pct(baseline_encode, fast_encode) << "\n";
    std::cout << "decode_latency_reduction_pct=" << reduction_pct(baseline_decode, fast_decode) << "\n";
    std::cout << "throughput_improvement_pct=" << -reduction_pct(baseline_throughput, fast_throughput) << "\n";
    std::cout << "alloc_count_reduction_pct=" << reduction_pct(static_cast<double>(baseline_allocations), static_cast<double>(fast_allocations)) << "\n";
    std::cout << "alloc_bytes_reduction_pct=" << reduction_pct(static_cast<double>(baseline_allocated_bytes), static_cast<double>(fast_allocated_bytes)) << "\n";

    return round_trip_failures == 0 ? 0 : 1;
}
