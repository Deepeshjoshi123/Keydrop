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
