#include <cassert>

#include "keydrop/core/encoder.hpp"
#include "keydrop/core/packet_reader.hpp"
#include "keydrop/schema/schema_runtime.hpp"

using namespace keydrop;

static void write_u16(Buffer& packet, u16 value)
{
    packet.write(static_cast<byte>(value & 0xFF));
    packet.write(static_cast<byte>((value >> 8) & 0xFF));
}

static void append_packet(Buffer& stream, const Buffer& packet)
{
    if (!packet.empty())
    {
        stream.append(packet.data().data(), packet.size());
    }
}

int main()
{
    SchemaRuntime runtime;

    const SchemaDef sensor_schema {
        "SensorData",
        7,
        {
            FieldDef {"temperature", FieldType::u8, 0, {}},
            FieldDef {"humidity", FieldType::u16, 1, {}},
            FieldDef {"device_id", FieldType::string, 2, FieldConstraints {true, 32}},
        }
    };

    const SchemaRegistryStatus reg_status = runtime.register_schema(sensor_schema);
    assert(reg_status.ok());

    BufferPoolConfig buffer_pool_config;
    buffer_pool_config.initial_buffers = 2;
    buffer_pool_config.default_capacity = 32;
    buffer_pool_config.max_available = 4;
    runtime.set_buffer_pool_config(buffer_pool_config);
    assert(runtime.buffer_pool_config().default_capacity == 32);

    PayloadPoolConfig payload_pool_config;
    payload_pool_config.initial_ordered_payloads = 2;
    payload_pool_config.initial_named_payloads = 2;
    payload_pool_config.default_field_capacity = 4;
    payload_pool_config.max_available = 4;
    runtime.set_payload_pool_config(payload_pool_config);
    assert(runtime.payload_pool_config().default_field_capacity == 4);

    const SchemaDef status_schema {
        "StatusData",
        9,
        {
            FieldDef {"status", FieldType::u8, 0, {}},
        }
    };
    assert(runtime.register_schema(status_schema).ok());

    NamedPayload payload;
    payload["device_id"] = FieldValue::from_string("sensor_01");
    payload["humidity"] = FieldValue::from_u16(700);
    payload["temperature"] = FieldValue::from_u8(32);

    Buffer packet;
    const SchemaRuntimeResult send_result = runtime.send("SensorData", payload, packet);
    assert(send_result.ok());
    assert(packet.size() > 2);

    PacketReader header_reader(packet);
    assert(header_reader.read_u16() == 7);

    OrderedPayload ordered_payload;
    ordered_payload.push_back(FieldValue::from_u8(32));
    ordered_payload.push_back(FieldValue::from_u16(700));
    ordered_payload.push_back(FieldValue::from_string("sensor_01"));
    Buffer ordered_packet;
    const SchemaRuntimeResult send_ordered_result =
        runtime.send_ordered("SensorData", ordered_payload, ordered_packet);
    assert(send_ordered_result.ok());
    assert(ordered_packet.data() == packet.data());

    std::string decoded_schema_name;
    NamedPayload decoded_payload;
    const SchemaRuntimeResult receive_result = runtime.receive(packet, decoded_schema_name, decoded_payload);
    assert(receive_result.ok());
    assert(decoded_schema_name == "SensorData");
    assert(decoded_payload.size() == 3);
    assert(decoded_payload["temperature"].as_u8 == 32);
    assert(decoded_payload["humidity"].as_u16 == 700);
    assert(decoded_payload["device_id"].as_string == "sensor_01");

    // Send failure: unknown schema
    const SchemaRuntimeResult send_unknown = runtime.send("Unknown", payload, packet);
    assert(send_unknown.code == SchemaRuntimeCode::schema_not_found);

    // Send failure: missing field
    NamedPayload missing_payload = payload;
    missing_payload.erase("humidity");
    const SchemaRuntimeResult send_missing = runtime.send("SensorData", missing_payload, packet);
    assert(send_missing.code == SchemaRuntimeCode::mapping_failed);

    // Send failure: schema field constraint violation
    NamedPayload long_payload = payload;
    long_payload["device_id"] = FieldValue::from_string("sensor_id_that_is_too_long_for_schema_limit");
    const SchemaRuntimeResult send_long = runtime.send("SensorData", long_payload, packet);
    assert(send_long.code == SchemaRuntimeCode::schema_mismatch);

    // Receive failure: short packet
    Buffer short_packet;
    short_packet.write(0x01);
    const SchemaRuntimeResult short_receive = runtime.receive(short_packet, decoded_schema_name, decoded_payload);
    assert(short_receive.code == SchemaRuntimeCode::packet_too_small);

    // Receive failure: unknown message_id
    Buffer unknown_id_packet;
    Encoder encoder;
    encoder.write_u16(999);
    unknown_id_packet = encoder.buffer();
    const SchemaRuntimeResult unknown_receive = runtime.receive(unknown_id_packet, decoded_schema_name, decoded_payload);
    assert(unknown_receive.code == SchemaRuntimeCode::schema_not_found);

    // Receive failure: explicit expected schema does not match packet message_id
    NamedPayload status_payload;
    status_payload["status"] = FieldValue::from_u8(1);
    Buffer status_packet;
    assert(runtime.send("StatusData", status_payload, status_packet).ok());
    const SchemaRuntimeResult receive_as_wrong_schema =
        runtime.receive_as("SensorData", status_packet, decoded_payload);
    assert(receive_as_wrong_schema.code == SchemaRuntimeCode::schema_mismatch);

    NamedPayload receive_as_payload;
    const SchemaRuntimeResult receive_as_ok =
        runtime.receive_as("StatusData", status_packet, receive_as_payload);
    assert(receive_as_ok.ok());
    assert(receive_as_payload["status"].as_u8 == 1);

    // Receive failure: trailing bytes detected before decode consumes fields
    Buffer trailing_packet = packet;
    trailing_packet.write(0xFF);
    const SchemaRuntimeResult trailing_receive = runtime.receive(trailing_packet, decoded_schema_name, decoded_payload);
    assert(trailing_receive.code == SchemaRuntimeCode::corruption_detected);

    // Receive failure: random byte corruption in a string length prefix
    Buffer corrupted_length_packet;
    write_u16(corrupted_length_packet, 7);
    corrupted_length_packet.write(32);
    write_u16(corrupted_length_packet, 700);
    write_u16(corrupted_length_packet, 0xFF00);
    corrupted_length_packet.write('s');
    const SchemaRuntimeResult corrupted_length_receive =
        runtime.receive(corrupted_length_packet, decoded_schema_name, decoded_payload);
    assert(corrupted_length_receive.code == SchemaRuntimeCode::corruption_detected);

    // Receive failure: truncated string payload
    Buffer truncated_string_packet;
    write_u16(truncated_string_packet, 7);
    truncated_string_packet.write(32);
    write_u16(truncated_string_packet, 700);
    write_u16(truncated_string_packet, 3);
    truncated_string_packet.write('a');
    truncated_string_packet.write('b');
    const SchemaRuntimeResult truncated_string_receive =
        runtime.receive(truncated_string_packet, decoded_schema_name, decoded_payload);
    assert(truncated_string_receive.code == SchemaRuntimeCode::corruption_detected);

    // Receive failure: decoded packet violates schema max-length constraint
    Buffer overlong_string_packet;
    write_u16(overlong_string_packet, 7);
    overlong_string_packet.write(32);
    write_u16(overlong_string_packet, 700);
    write_u16(overlong_string_packet, 33);
    for (usize i = 0; i < 33; ++i)
    {
        overlong_string_packet.write('x');
    }
    const SchemaRuntimeResult overlong_string_receive =
        runtime.receive(overlong_string_packet, decoded_schema_name, decoded_payload);
    assert(overlong_string_receive.code == SchemaRuntimeCode::corruption_detected);

    const SchemaDef binary_schema {
        "BinaryData",
        8,
        {
            FieldDef {"raw", FieldType::bytes, 0, FieldConstraints {true, 8}},
        }
    };
    assert(runtime.register_schema(binary_schema).ok());

    // Receive failure: truncated bytes payload
    Buffer truncated_bytes_packet;
    write_u16(truncated_bytes_packet, 8);
    write_u16(truncated_bytes_packet, 4);
    truncated_bytes_packet.write(0x01);
    truncated_bytes_packet.write(0x02);
    const SchemaRuntimeResult truncated_bytes_receive =
        runtime.receive(truncated_bytes_packet, decoded_schema_name, decoded_payload);
    assert(truncated_bytes_receive.code == SchemaRuntimeCode::corruption_detected);

    // Packet synchronization recovery: skip corrupted bytes and decode valid packets.
    NamedPayload payload_recovery_a = payload;
    payload_recovery_a["temperature"] = FieldValue::from_u8(44);
    Buffer recovery_packet_a;
    assert(runtime.send("SensorData", payload_recovery_a, recovery_packet_a).ok());

    NamedPayload payload_recovery_b = payload;
    payload_recovery_b["temperature"] = FieldValue::from_u8(45);
    Buffer recovery_packet_b;
    assert(runtime.send("SensorData", payload_recovery_b, recovery_packet_b).ok());

    Buffer recovery_stream;
    recovery_stream.write(0xAA);
    recovery_stream.write(0xBB);
    append_packet(recovery_stream, recovery_packet_a);
    recovery_stream.write(0x07);
    recovery_stream.write(0x00);
    recovery_stream.write(0xFF);
    append_packet(recovery_stream, recovery_packet_b);

    std::vector<std::pair<std::string, NamedPayload>> recovered_messages;
    usize skipped_bytes = 0;
    const SchemaRuntimeResult recovered_stream_result =
        runtime.receive_recovered_stream(recovery_stream, recovered_messages, skipped_bytes);
    assert(recovered_stream_result.ok());
    assert(skipped_bytes == 5);
    assert(recovered_messages.size() == 2);
    assert(recovered_messages[0].first == "SensorData");
    assert(recovered_messages[0].second["temperature"].as_u8 == 44);
    assert(recovered_messages[1].second["temperature"].as_u8 == 45);

    Buffer unrecoverable_stream;
    unrecoverable_stream.write(0xAA);
    unrecoverable_stream.write(0xBB);
    const SchemaRuntimeResult unrecoverable_result =
        runtime.receive_recovered_stream(unrecoverable_stream, recovered_messages, skipped_bytes);
    assert(unrecoverable_result.code == SchemaRuntimeCode::synchronization_failed);

    // JSON API success path
    JsonObject json_payload;
    json_payload["temperature"] = JsonValue::from_integer(32);
    json_payload["humidity"] = JsonValue::from_integer(700);
    json_payload["device_id"] = JsonValue::from_string("sensor_01");

    Buffer json_packet;
    const SchemaRuntimeResult json_send_ok = runtime.send_json("SensorData", json_payload, json_packet);
    assert(json_send_ok.ok());

    JsonObject json_decoded;
    const SchemaRuntimeResult json_receive_ok = runtime.receive_json(json_packet, decoded_schema_name, json_decoded);
    assert(json_receive_ok.ok());
    assert(decoded_schema_name == "SensorData");
    assert(json_decoded.size() == 3);
    assert(json_decoded["temperature"].type == JsonValueType::integer);
    assert(json_decoded["temperature"].integer_value == 32);
    assert(json_decoded["humidity"].integer_value == 700);
    assert(json_decoded["device_id"].string_value == "sensor_01");

    // JSON API failure: incompatible type
    JsonObject bad_json = json_payload;
    bad_json["humidity"] = JsonValue::from_string("bad");
    const SchemaRuntimeResult json_bad_type = runtime.send_json("SensorData", bad_json, json_packet);
    assert(json_bad_type.code == SchemaRuntimeCode::json_conversion_failed);

    // JSON API failure: unknown field
    JsonObject extra_json = json_payload;
    extra_json["extra"] = JsonValue::from_integer(1);
    const SchemaRuntimeResult json_extra = runtime.send_json("SensorData", extra_json, json_packet);
    assert(json_extra.code == SchemaRuntimeCode::json_conversion_failed);

    return 0;
}
