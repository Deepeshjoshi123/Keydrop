// Phase 5 fuzz test: no crash, no out-of-bounds access, no uncaught
// exception when arbitrary and mutated bytes reach every receive path.
// All runtime receive APIs must return a structured result; this test
// only asserts that control returns (the sanitizer builds verify memory
// safety). Deterministic xorshift generator — no external RNG.

#include <cassert>
#include <string>
#include <vector>

#include "keydrop/schema/schema_runtime.hpp"

using namespace keydrop;

namespace {

u32 g_state = 0x5EED1234u;

u32 next_rand()
{
    u32 x = g_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_state = x;
    return x;
}

byte random_byte()
{
    return static_cast<byte>(next_rand() & 0xFF);
}

usize random_size(usize bound)
{
    return bound == 0 ? 0 : static_cast<usize>(next_rand() % (bound + 1));
}

void feed_random(const Buffer& buffer, SchemaRuntime& runtime)
{
    std::string schema_name;
    NamedPayload named;
    OrderedPayload ordered;
    std::vector<std::pair<std::string, NamedPayload>> messages;
    usize skipped = 0;
    FastDecodedField fast_fields[16];
    usize fast_count = 0;
    JsonObject json;

    // Every call must return without throwing or crashing. The result code
    // itself is unconstrained: ok, decode_failed, corruption_detected,
    // schema_not_found, and packet_too_small are all valid outcomes.
    (void)runtime.receive(buffer, schema_name, named);
    (void)runtime.receive_ordered(buffer, schema_name, ordered);
    (void)runtime.fast_decode(buffer, schema_name, fast_fields, 16, fast_count);
    (void)runtime.receive_stream(buffer, messages);
    (void)runtime.receive_recovered_stream(buffer, messages, skipped);
    (void)runtime.receive_json(buffer, schema_name, json);
}

SchemaRuntime make_fuzz_runtime()
{
    SchemaRuntime runtime;
    const SchemaDef schema {
        "Fuzz",
        88,
        {
            FieldDef {"a", FieldType::u8, 0, {}},
            FieldDef {"b", FieldType::u16, 1, {}},
            FieldDef {"c", FieldType::u32, 2, {}},
            FieldDef {"d", FieldType::i16, 3, {}},
            FieldDef {"s", FieldType::string, 4, FieldConstraints {true, 32}},
            FieldDef {"raw", FieldType::bytes, 5, FieldConstraints {true, 16}},
        }
    };
    (void)runtime.register_schema(schema);

    // Enable everything so all decode branches are exercised: dictionary
    // references, delta packets, optimized packets, CRC envelopes.
    AdaptiveDictionaryConfig dict;
    dict.enabled = true;
    dict.enable_string_values = true;
    runtime.set_dictionary_config(dict);

    RuntimeOptimizerConfig optimizer;
    optimizer.enabled = true;
    optimizer.enable_zero_value_omission = true;
    runtime.set_optimizer_config(optimizer);

    StreamOptimizerConfig stream;
    stream.enabled = true;
    stream.enable_packet_reuse = true;
    stream.enable_delta_updates = true;
    stream.enable_batching = true;
    stream.enable_delta_packets = true;
    stream.keyframe_interval = 50;
    stream.aggressive_after_samples = 1;
    runtime.set_stream_optimizer_config(stream);
    runtime.reset_stream_optimizer();

    ReliabilityConfig reliability;
    reliability.enable_crc32 = true;
    reliability.max_recovered_packets = 64;
    runtime.set_reliability_config(reliability);

    return runtime;
}

NamedPayload random_payload()
{
    NamedPayload p;
    p["a"] = FieldValue::from_u8(random_byte());
    p["b"] = FieldValue::from_u16(static_cast<u16>(next_rand()));
    p["c"] = FieldValue::from_u32(next_rand());
    p["d"] = FieldValue::from_i16(static_cast<i16>(next_rand()));
    std::string s;
    for (usize i = 0; i < random_size(8); ++i)
    {
        s += static_cast<char>('a' + (next_rand() % 26));
    }
    p["s"] = FieldValue::from_string(s);
    std::vector<byte> raw;
    for (usize i = 0; i < random_size(4); ++i)
    {
        raw.push_back(random_byte());
    }
    p["raw"] = FieldValue::from_bytes(raw);
    return p;
}

} // namespace

int main()
{
    SchemaRuntime runtime = make_fuzz_runtime();

    // ── 1. Truly random buffers through every receive path ───────
    for (usize i = 0; i < 20000; ++i)
    {
        Buffer buffer;
        for (usize j = 0; j < random_size(64); ++j)
        {
            buffer.write(random_byte());
        }
        feed_random(buffer, runtime);
    }

    // ── 2. Valid packets, mutated, through every receive path ────
    // Build a bank of valid outputs from this runtime: plain, stream,
    // delta, batch, CRC-wrapped.
    std::vector<Buffer> valid_packets;
    for (usize i = 0; i < 200; ++i)
    {
        const NamedPayload payload = random_payload();
        Buffer plain;
        if (runtime.send("Fuzz", payload, plain).ok())
        {
            valid_packets.push_back(plain);
        }
        Buffer streamed;
        bool has = false;
        if (runtime.send_stream("Fuzz", payload, streamed, has).ok() && has)
        {
            valid_packets.push_back(streamed);
        }
    }
    Buffer flushed;
    bool flushed_has = false;
    if (runtime.flush_stream(flushed, flushed_has).ok() && flushed_has)
    {
        valid_packets.push_back(flushed);
    }

    for (usize i = 0; i < 20000; ++i)
    {
        Buffer mutated = valid_packets[next_rand() % valid_packets.size()];
        const usize mutation_kind = random_size(4);
        if (mutation_kind == 0)
        {
            // Bit flip
            if (!mutated.empty())
            {
                const usize index = random_size(mutated.size() - 1);
                const std::vector<byte>& data = mutated.data();
                Buffer rebuilt;
                for (usize j = 0; j < mutated.size(); ++j)
                {
                    rebuilt.write(j == index ? static_cast<byte>(data[j] ^ 0x40) : data[j]);
                }
                mutated = rebuilt;
            }
        }
        else if (mutation_kind == 1)
        {
            // Truncation
            Buffer rebuilt;
            const usize keep = mutated.empty() ? 0 : random_size(mutated.size() - 1);
            rebuilt.append(mutated.data().data(), keep);
            mutated = rebuilt;
        }
        else if (mutation_kind == 2)
        {
            // Extension with garbage
            for (usize j = 0; j < random_size(8); ++j)
            {
                mutated.write(random_byte());
            }
        }
        else if (mutation_kind == 3 && !mutated.empty())
        {
            // Random overwrite of a span
            const usize offset = random_size(mutated.size() - 1);
            const usize span = random_size(mutated.size() - offset - 1);
            std::vector<byte> data = mutated.data();
            for (usize j = 0; j < span; ++j)
            {
                data[offset + j] = random_byte();
            }
            Buffer rebuilt;
            rebuilt.append(data.data(), data.size());
            mutated = rebuilt;
        }

        feed_random(mutated, runtime);
    }

    // ── 3. CRC gate: corrupted stream packet rejected, never decoded ──
    {
        Buffer wrapped;
        bool has = false;
        assert(runtime.send_stream("Fuzz", random_payload(), wrapped, has).ok() && has);
        assert(!wrapped.empty() && wrapped.data()[0] == kCrcWrapperMarker);
        std::vector<std::pair<std::string, NamedPayload>> messages;
        assert(runtime.receive_stream(wrapped, messages).ok());

        Buffer corrupted;
        const std::vector<byte>& data = wrapped.data();
        const usize index = wrapped.size() - 1;
        for (usize j = 0; j < wrapped.size(); ++j)
        {
            corrupted.write(j == index ? static_cast<byte>(data[j] ^ 0x01) : data[j]);
        }
        const SchemaRuntimeResult bad = runtime.receive_stream(corrupted, messages);
        assert(bad.code == SchemaRuntimeCode::corruption_detected);
        assert(messages.empty()); // nothing was decoded from the corrupted packet
    }

    // ── 4. Control- and envelope-prefixed garbage ────────────────
    for (usize i = 0; i < 2000; ++i)
    {
        Buffer buffer;
        buffer.write(random_size(3) == 0 ? 0xFA : (random_size(3) == 1 ? 0xFB : (random_size(3) == 2 ? 0xFC : 0xF9)));
        for (usize j = 0; j < random_size(32); ++j)
        {
            buffer.write(random_byte());
        }
        feed_random(buffer, runtime);
    }

    return 0;
}
