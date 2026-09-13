#include "keydrop/benchmark/format_benchmark.hpp"

#include <cstdlib>
#include <sstream>

#include "keydrop/benchmark/heap_tracker.hpp"
#include "keydrop/schema/schema_runtime.hpp"

namespace keydrop {

namespace {

constexpr u16 kBenchmarkMessageId = 91;

void write_u8(Buffer& buffer, u8 value)
{
    buffer.write(value);
}

void write_u16(Buffer& buffer, u16 value)
{
    buffer.write(static_cast<byte>(value & 0xFF));
    buffer.write(static_cast<byte>((value >> 8) & 0xFF));
}

void write_string(Buffer& buffer, const std::string& value)
{
    write_u16(buffer, static_cast<u16>(value.size()));
    if (!value.empty())
    {
        buffer.append(
            reinterpret_cast<const byte*>(value.data()),
            value.size()
        );
    }
}

bool read_u8(const Buffer& buffer, usize& cursor, u8& out)
{
    if (cursor + 1 > buffer.size())
    {
        return false;
    }

    out = buffer.data()[cursor];
    cursor += 1;
    return true;
}

bool read_u16(const Buffer& buffer, usize& cursor, u16& out)
{
    if (cursor + 2 > buffer.size())
    {
        return false;
    }

    out = static_cast<u16>(buffer.data()[cursor])
        | static_cast<u16>(static_cast<u16>(buffer.data()[cursor + 1]) << 8);
    cursor += 2;
    return true;
}

bool read_string(const Buffer& buffer, usize& cursor, std::string& out)
{
    u16 size = 0;
    if (!read_u16(buffer, cursor, size))
    {
        return false;
    }

    if (cursor + size > buffer.size())
    {
        return false;
    }

    out.assign(
        reinterpret_cast<const char*>(&buffer.data()[cursor]),
        size
    );
    cursor += size;
    return true;
}

Buffer string_to_buffer(const std::string& value)
{
    Buffer buffer;
    if (!value.empty())
    {
        buffer.append(
            reinterpret_cast<const byte*>(value.data()),
            value.size()
        );
    }
    return buffer;
}

std::string buffer_to_string(const Buffer& buffer)
{
    if (buffer.empty())
    {
        return std::string();
    }

    return std::string(
        reinterpret_cast<const char*>(buffer.data().data()),
        buffer.size()
    );
}

SchemaRuntime make_benchmark_runtime()
{
    SchemaRuntime runtime;
    const SchemaDef schema {
        "BenchmarkPayload",
        kBenchmarkMessageId,
        {
            FieldDef {"temperature", FieldType::u16, 0, {}},
            FieldDef {"humidity", FieldType::u16, 1, {}},
            FieldDef {"device_id", FieldType::string, 2, FieldConstraints {true, 64}},
        }
    };
    (void)runtime.register_schema(schema);
    return runtime;
}

const SchemaRuntime& benchmark_runtime()
{
    static const SchemaRuntime runtime = make_benchmark_runtime();
    return runtime;
}

OrderedPayload to_ordered_payload(const BenchmarkPayload& payload)
{
    OrderedPayload ordered;
    ordered.reserve(3);
    ordered.push_back(FieldValue::from_u16(payload.temperature));
    ordered.push_back(FieldValue::from_u16(payload.humidity));
    ordered.push_back(FieldValue::from_string(payload.device_id));
    return ordered;
}

bool from_named_payload(const NamedPayload& named, BenchmarkPayload& out_payload)
{
    const NamedPayload::const_iterator temp_it = named.find("temperature");
    const NamedPayload::const_iterator humidity_it = named.find("humidity");
    const NamedPayload::const_iterator device_it = named.find("device_id");
    if (
        temp_it == named.end()
        ||
        humidity_it == named.end()
        ||
        device_it == named.end()
    )
    {
        return false;
    }

    out_payload.temperature = temp_it->second.as_u16;
    out_payload.humidity = humidity_it->second.as_u16;
    out_payload.device_id = device_it->second.as_string;
    return true;
}

bool extract_json_string(
    const std::string& json,
    const std::string& key,
    std::string& out_value
)
{
    const std::string marker = "\"" + key + "\":\"";
    const std::size_t start = json.find(marker);
    if (start == std::string::npos)
    {
        return false;
    }

    const std::size_t value_start = start + marker.size();
    const std::size_t value_end = json.find('"', value_start);
    if (value_end == std::string::npos)
    {
        return false;
    }

    out_value = json.substr(value_start, value_end - value_start);
    return true;
}

bool extract_json_u16(
    const std::string& json,
    const std::string& key,
    u16& out_value
)
{
    const std::string marker = "\"" + key + "\":";
    const std::size_t start = json.find(marker);
    if (start == std::string::npos)
    {
        return false;
    }

    const std::size_t value_start = start + marker.size();
    const char* raw = json.c_str() + value_start;
    char* end = nullptr;
    const long parsed = std::strtol(raw, &end, 10);
    if (end == raw || parsed < 0 || parsed > 65535)
    {
        return false;
    }

    out_value = static_cast<u16>(parsed);
    return true;
}

BenchmarkSample run_benchmark(
    const std::string& name,
    usize iterations,
    EncodedPayload (*encode)(const BenchmarkPayload&),
    bool (*decode)(const Buffer&, BenchmarkPayload&)
)
{
    const BenchmarkPayload payload = default_benchmark_payload();
    BenchmarkSample sample;
    sample.name = name;
    sample.iterations = iterations;

    // Warm-up: one encode to populate caches, schema registries, etc.
    EncodedPayload first = encode(payload);
    sample.packet_size_bytes = first.bytes.size();

    // ── Measure actual heap allocations during encode ──────────
    HeapTracker::reset();
    HeapTracker::begin();
    BenchmarkTimer timer;
    for (usize i = 0; i < iterations; ++i)
    {
        EncodedPayload encoded = encode(payload);
    }
    sample.encode_time_ns = timer.elapsed_ns();
    HeapTracker::end();
    sample.allocations = HeapTracker::allocation_count();
    sample.allocated_bytes = HeapTracker::allocated_bytes();

    // ── Decode timing (no allocation tracking needed) ──────────
    timer.reset();
    for (usize i = 0; i < iterations; ++i)
    {
        BenchmarkPayload decoded;
        if (!decode(first.bytes, decoded))
        {
            break;
        }
    }
    sample.decode_time_ns = timer.elapsed_ns();
    return sample;
}

} // namespace

BenchmarkPayload default_benchmark_payload()
{
    return BenchmarkPayload{};
}

bool payloads_equal(const BenchmarkPayload& left, const BenchmarkPayload& right)
{
    return left.temperature == right.temperature
        && left.humidity == right.humidity
        && left.device_id == right.device_id;
}

EncodedPayload encode_keydrop_payload(const BenchmarkPayload& payload)
{
    Buffer packet;
    (void)benchmark_runtime().send_ordered("BenchmarkPayload", to_ordered_payload(payload), packet);
    return {"keydrop", packet};
}

EncodedPayload encode_json_payload(const BenchmarkPayload& payload)
{
    std::ostringstream json;
    json << "{\"temperature\":" << payload.temperature
         << ",\"humidity\":" << payload.humidity
         << ",\"device_id\":\"" << payload.device_id << "\"}";
    return {"json", string_to_buffer(json.str())};
}

EncodedPayload encode_protobuf_payload(const BenchmarkPayload& payload)
{
    Buffer buffer;
    write_u8(buffer, 0x08);
    write_u16(buffer, payload.temperature);
    write_u8(buffer, 0x10);
    write_u16(buffer, payload.humidity);
    write_u8(buffer, 0x1A);
    write_string(buffer, payload.device_id);
    return {"protobuf", buffer};
}

EncodedPayload encode_messagepack_payload(const BenchmarkPayload& payload)
{
    Buffer buffer;
    write_u8(buffer, 0x83); // map with three entries
    write_u8(buffer, 0xAB); // 11-byte key
    buffer.append(reinterpret_cast<const byte*>("temperature"), 11);
    write_u16(buffer, payload.temperature);
    write_u8(buffer, 0xA8); // 8-byte key
    buffer.append(reinterpret_cast<const byte*>("humidity"), 8);
    write_u16(buffer, payload.humidity);
    write_u8(buffer, 0xA9); // 9-byte key
    buffer.append(reinterpret_cast<const byte*>("device_id"), 9);
    write_string(buffer, payload.device_id);
    return {"messagepack", buffer};
}

bool decode_keydrop_payload(const Buffer& bytes, BenchmarkPayload& out_payload)
{
    std::string schema_name;
    OrderedPayload ordered;
    if (!benchmark_runtime().receive_ordered(bytes, schema_name, ordered).ok())
    {
        return false;
    }

    if (schema_name != "BenchmarkPayload" || ordered.size() < 3)
    {
        return false;
    }

    out_payload.temperature = ordered[0].as_u16;
    out_payload.humidity = ordered[1].as_u16;
    out_payload.device_id = ordered[2].as_string;
    return true;
}

bool decode_json_payload(const Buffer& bytes, BenchmarkPayload& out_payload)
{
    const std::string json = buffer_to_string(bytes);
    return extract_json_u16(json, "temperature", out_payload.temperature)
        && extract_json_u16(json, "humidity", out_payload.humidity)
        && extract_json_string(json, "device_id", out_payload.device_id);
}

bool decode_protobuf_payload(const Buffer& bytes, BenchmarkPayload& out_payload)
{
    usize cursor = 0;
    u8 tag = 0;
    if (!read_u8(bytes, cursor, tag) || tag != 0x08)
    {
        return false;
    }
    if (!read_u16(bytes, cursor, out_payload.temperature))
    {
        return false;
    }
    if (!read_u8(bytes, cursor, tag) || tag != 0x10)
    {
        return false;
    }
    if (!read_u16(bytes, cursor, out_payload.humidity))
    {
        return false;
    }
    if (!read_u8(bytes, cursor, tag) || tag != 0x1A)
    {
        return false;
    }
    if (!read_string(bytes, cursor, out_payload.device_id))
    {
        return false;
    }

    return cursor == bytes.size();
}

bool decode_messagepack_payload(const Buffer& bytes, BenchmarkPayload& out_payload)
{
    usize cursor = 0;
    u8 marker = 0;
    if (!read_u8(bytes, cursor, marker) || marker != 0x83)
    {
        return false;
    }

    if (!read_u8(bytes, cursor, marker) || marker != 0xAB)
    {
        return false;
    }
    if (cursor + 11 > bytes.size())
    {
        return false;
    }
    cursor += 11;
    if (!read_u16(bytes, cursor, out_payload.temperature))
    {
        return false;
    }

    if (!read_u8(bytes, cursor, marker) || marker != 0xA8)
    {
        return false;
    }
    if (cursor + 8 > bytes.size())
    {
        return false;
    }
    cursor += 8;
    if (!read_u16(bytes, cursor, out_payload.humidity))
    {
        return false;
    }

    if (!read_u8(bytes, cursor, marker) || marker != 0xA9)
    {
        return false;
    }
    if (cursor + 9 > bytes.size())
    {
        return false;
    }
    cursor += 9;
    if (!read_string(bytes, cursor, out_payload.device_id))
    {
        return false;
    }

    return cursor == bytes.size();
}

BenchmarkSample run_keydrop_benchmark(usize iterations)
{
    return run_benchmark("keydrop", iterations, encode_keydrop_payload, decode_keydrop_payload);
}

BenchmarkSample run_json_benchmark(usize iterations)
{
    return run_benchmark("json", iterations, encode_json_payload, decode_json_payload);
}

BenchmarkSample run_protobuf_benchmark(usize iterations)
{
    return run_benchmark("protobuf", iterations, encode_protobuf_payload, decode_protobuf_payload);
}

BenchmarkSample run_messagepack_benchmark(usize iterations)
{
    return run_benchmark("messagepack", iterations, encode_messagepack_payload, decode_messagepack_payload);
}

std::vector<BenchmarkSample> run_format_benchmarks(usize iterations)
{
    std::vector<BenchmarkSample> samples;
    samples.push_back(run_keydrop_benchmark(iterations));
    samples.push_back(run_json_benchmark(iterations));
    samples.push_back(run_protobuf_benchmark(iterations));
    samples.push_back(run_messagepack_benchmark(iterations));
    return samples;
}

}
