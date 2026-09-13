#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

#include "keydrop/core/packet_reader.hpp"
#include "keydrop/schema/fast_codec.hpp"
#include "keydrop/schema/schema_runtime.hpp"

using namespace keydrop;

int main()
{
    // ── Schema covering all ten field types ──────────────────────
    SchemaRuntime runtime;
    const SchemaDef all_types {
        "AllTypes",
        71,
        {
            FieldDef {"a_u8", FieldType::u8, 0, {}},
            FieldDef {"b_u16", FieldType::u16, 1, {}},
            FieldDef {"c_u32", FieldType::u32, 2, {}},
            FieldDef {"d_i8", FieldType::i8, 3, {}},
            FieldDef {"e_i16", FieldType::i16, 4, {}},
            FieldDef {"f_i32", FieldType::i32, 5, {}},
            FieldDef {"g_f32", FieldType::f32, 6, {}},
            FieldDef {"h_f64", FieldType::f64, 7, {}},
            FieldDef {"i_str", FieldType::string, 8, FieldConstraints {true, 32}},
            FieldDef {"j_bytes", FieldType::bytes, 9, FieldConstraints {true, 16}},
        }
    };
    assert(runtime.register_schema(all_types).ok());

    FieldValue values[10];
    values[0] = FieldValue::from_u8(250);
    values[1] = FieldValue::from_u16(60000);
    values[2] = FieldValue::from_u32(4000000000u);
    values[3] = FieldValue::from_i8(-5);
    values[4] = FieldValue::from_i16(-12345);
    values[5] = FieldValue::from_i32(-1000000);
    values[6] = FieldValue::from_f32(3.5f);
    values[7] = FieldValue::from_f64(-2.25);
    values[8] = FieldValue::from_string("sensor_01");
    values[9] = FieldValue::from_bytes({0x01, 0x02, 0x03});

    // ── Fast encode → fast decode round-trip ─────────────────────
    Buffer packet;
    assert(runtime.fast_encode("AllTypes", values, 10, packet).ok());

    std::string schema_name;
    FastDecodedField fields[10];
    usize count = 0;
    const SchemaRuntimeResult decoded = runtime.fast_decode(packet, schema_name, fields, 10, count);
    assert(decoded.ok());
    assert(schema_name == "AllTypes" && count == 10);
    assert(fields[0].as_integer == 250);
    assert(fields[1].as_integer == 60000);
    assert(fields[2].as_integer == 4000000000u);
    assert(static_cast<i8>(fields[3].as_integer) == -5);
    assert(static_cast<i16>(fields[4].as_integer) == -12345);
    assert(static_cast<i32>(fields[5].as_integer) == -1000000);
    assert(fields[6].as_float == 3.5);
    assert(fields[7].as_float == -2.25);
    assert(fields[8].type == FieldType::string && fields[8].owned_string == false);
    assert(fields[8].view.size() == 9);
    assert(fields[9].type == FieldType::bytes && fields[9].view.size() == 3);

    // ── Zero-copy: string/bytes views point inside the packet ────
    const byte* packet_begin = packet.bytes();
    const byte* packet_end = packet_begin + packet.size();
    assert(fields[8].view.bytes() >= packet_begin && fields[8].view.bytes() + fields[8].view.size() <= packet_end);
    assert(fields[9].view.bytes() >= packet_begin && fields[9].view.bytes() + fields[9].view.size() <= packet_end);

    // ── Fast packet is byte-identical to the general stateless path
    OrderedPayload ordered;
    for (usize i = 0; i < 10; ++i)
    {
        ordered.push_back(values[i]);
    }
    Buffer general_packet;
    assert(runtime.send_ordered("AllTypes", ordered, general_packet).ok());
    assert(packet.data() == general_packet.data());

    // ── Fast packets decode through the general path (interop) ───
    OrderedPayload general_decoded;
    assert(runtime.receive_ordered(packet, schema_name, general_decoded).ok());
    assert(general_decoded.size() == 10 && general_decoded[8].as_string == "sensor_01");

    // ── Caller buffer capacity is reused across encodes ──────────
    Buffer reused;
    reused.reserve(64);
    assert(runtime.fast_encode("AllTypes", values, 10, reused).ok());
    const usize capacity_after_first = reused.capacity();
    for (usize i = 0; i < 8; ++i)
    {
        assert(runtime.fast_encode("AllTypes", values, 10, reused).ok());
    }
    assert(reused.capacity() == capacity_after_first);

    // ── Error: field count / type mismatch ───────────────────────
    assert(runtime.fast_encode("AllTypes", values, 9, reused).code == SchemaRuntimeCode::mapping_failed);
    FieldValue bad_values[10];
    for (usize i = 0; i < 10; ++i)
    {
        bad_values[i] = values[i];
    }
    bad_values[2] = FieldValue::from_u16(7); // wrong type for c_u32
    assert(runtime.fast_encode("AllTypes", bad_values, 10, reused).code == SchemaRuntimeCode::mapping_failed);
    assert(runtime.fast_encode("Unknown", values, 10, reused).code == SchemaRuntimeCode::schema_not_found);

    // ── Error: truncated packet and trailing bytes ───────────────
    Buffer truncated;
    truncated.append(packet.bytes(), packet.size() / 2);
    assert(runtime.fast_decode(truncated, schema_name, fields, 10, count).code == SchemaRuntimeCode::decode_failed);

    Buffer trailing = packet;
    trailing.write(0xFF);
    assert(runtime.fast_decode(trailing, schema_name, fields, 10, count).code == SchemaRuntimeCode::decode_failed);

    Buffer tiny;
    tiny.write(0x01);
    assert(runtime.fast_decode(tiny, schema_name, fields, 10, count).code == SchemaRuntimeCode::packet_too_small);

    // ── Error: insufficient storage and unknown message_id ───────
    assert(runtime.fast_decode(packet, schema_name, fields, 5, count).code == SchemaRuntimeCode::decode_failed);
    Buffer foreign;
    foreign.write(0x63);
    foreign.write(0x00); // message_id 99, not registered
    assert(runtime.fast_decode(foreign, schema_name, fields, 10, count).code == SchemaRuntimeCode::schema_not_found);

    // ── Error: batch envelope rejected with a clear hint ─────────
    Buffer batch;
    batch.write(0xFC);
    batch.write(0x01);
    assert(runtime.fast_decode(batch, schema_name, fields, 10, count).code == SchemaRuntimeCode::decode_failed);

    // ── Dictionary references resolve into owned strings ─────────
    SchemaRuntime dict_runtime;
    const SchemaDef dict_schema {
        "Dict",
        73,
        {
            FieldDef {"id", FieldType::string, 0, FieldConstraints {true, 64}},
            FieldDef {"v", FieldType::u16, 1, {}},
        }
    };
    assert(dict_runtime.register_schema(dict_schema).ok());
    AdaptiveDictionaryConfig dict_config;
    dict_config.enabled = true;
    dict_config.enable_string_values = true;
    dict_runtime.set_dictionary_config(dict_config);

    NamedPayload dict_payload;
    dict_payload["id"] = FieldValue::from_string("device-42");
    dict_payload["v"] = FieldValue::from_u16(1);
    Buffer dict_first;
    Buffer dict_second;
    assert(dict_runtime.send("Dict", dict_payload, dict_first).ok());
    assert(dict_runtime.send("Dict", dict_payload, dict_second).ok());
    assert(dict_second.size() < dict_first.size()); // reference is shorter

    FastDecodedField dict_fields[2];
    usize dict_count = 0;
    const SchemaRuntimeResult dict_decoded =
        dict_runtime.fast_decode(dict_second, schema_name, dict_fields, 2, dict_count);
    assert(dict_decoded.ok());
    assert(dict_fields[0].owned_string && dict_fields[0].owned == "device-42");
    assert(dict_fields[1].as_integer == 1);

    // ── Optimized packets (0xFD) decode through the fast path ────
    SchemaRuntime opt_runtime;
    assert(opt_runtime.register_schema(all_types).ok());
    RuntimeOptimizerConfig opt_config;
    opt_config.enabled = true;
    opt_config.enable_zero_value_omission = true;
    opt_runtime.set_optimizer_config(opt_config);

    OrderedPayload opt_ordered;
    for (usize i = 0; i < 10; ++i)
    {
        opt_ordered.push_back(values[i]);
    }
    opt_ordered[0] = FieldValue::from_u8(0);  // omitted by the optimizer
    opt_ordered[2] = FieldValue::from_u32(0); // omitted by the optimizer
    Buffer optimized_packet;
    assert(opt_runtime.send_ordered("AllTypes", opt_ordered, optimized_packet).ok());
    assert(optimized_packet.size() >= 3 && optimized_packet.data()[2] == 0xFD);

    FastDecodedField opt_fields[10];
    usize opt_count = 0;
    const SchemaRuntimeResult opt_decoded =
        opt_runtime.fast_decode(optimized_packet, schema_name, opt_fields, 10, opt_count);
    assert(opt_decoded.ok());
    assert(opt_fields[0].as_integer == 0);
    assert(opt_fields[2].as_integer == 0);
    assert(opt_fields[1].as_integer == 60000);
    assert(static_cast<i8>(opt_fields[3].as_integer) == -5);
    assert(opt_fields[8].owned_string == false && opt_fields[8].view.size() == 9);
    const byte* opt_begin = optimized_packet.bytes();
    const byte* opt_end = opt_begin + optimized_packet.size();
    assert(opt_fields[8].view.bytes() >= opt_begin && opt_fields[8].view.bytes() + opt_fields[8].view.size() <= opt_end);

    // ── Repeated round-trips: zero failures ──────────────────────
    usize failures = 0;
    for (usize iter = 0; iter < 1000; ++iter)
    {
        FieldValue loop_values[10];
        loop_values[0] = FieldValue::from_u8(static_cast<u8>(iter % 251));
        loop_values[1] = FieldValue::from_u16(static_cast<u16>(iter * 7 % 60000));
        loop_values[2] = FieldValue::from_u32(static_cast<u32>(iter * 97 + 1000));
        loop_values[3] = FieldValue::from_i8(static_cast<i8>(iter % 200 - 100));
        loop_values[4] = FieldValue::from_i16(static_cast<i16>(iter * 3 - 500));
        loop_values[5] = FieldValue::from_i32(static_cast<i32>(iter) - 500);
        loop_values[6] = FieldValue::from_f32(static_cast<f32>(iter) * 0.5f);
        loop_values[7] = FieldValue::from_f64(static_cast<f64>(iter) * 0.25);
        loop_values[8] = FieldValue::from_string(iter % 3 == 0 ? "" : "loop_value");
        loop_values[9] = FieldValue::from_bytes({static_cast<byte>(iter % 251), 0x02});

        Buffer loop_packet;
        if (!runtime.fast_encode("AllTypes", loop_values, 10, loop_packet).ok())
        {
            ++failures;
            continue;
        }

        FastDecodedField loop_fields[10];
        usize loop_count = 0;
        if (!runtime.fast_decode(loop_packet, schema_name, loop_fields, 10, loop_count).ok())
        {
            ++failures;
            continue;
        }

        if (loop_fields[0].as_integer != loop_values[0].as_u8
            || loop_fields[1].as_integer != loop_values[1].as_u16
            || loop_fields[2].as_integer != loop_values[2].as_u32
            || static_cast<i8>(loop_fields[3].as_integer) != loop_values[3].as_i8
            || static_cast<i16>(loop_fields[4].as_integer) != loop_values[4].as_i16
            || static_cast<i32>(loop_fields[5].as_integer) != loop_values[5].as_i32
            || loop_fields[6].as_float != loop_values[6].as_f32
            || loop_fields[7].as_float != loop_values[7].as_f64
            || loop_fields[8].view.size() != loop_values[8].as_string.size()
            || loop_fields[9].view.size() != loop_values[9].as_bytes.size())
        {
            ++failures;
        }
    }
    assert(failures == 0);

    // ── PacketReader::skip() and buffer() ────────────────────────
    PacketReader reader(packet);
    assert(reader.read_u16() == 71);
    reader.skip(1); // a_u8
    reader.skip(2); // b_u16
    assert(reader.position() == 5);
    assert(reader.buffer().size() == packet.size());
    bool skip_threw = false;
    try
    {
        reader.skip(100000);
    }
    catch (const std::out_of_range&)
    {
        skip_threw = true;
    }
    assert(skip_threw);

    return 0;
}
