#include "keydrop/schema/schema_runtime.hpp"

#include <limits>
#include <deque>
#include <stdexcept>
#include <vector>

#include "keydrop/core/encoder.hpp"
#include "keydrop/core/packet_reader.hpp"
#include "keydrop/reliability/corruption_detector.hpp"

namespace keydrop {

namespace {
constexpr u16 kDictionaryStringReferenceMarker = 0xFFFF;

FieldValue read_field_value(PacketReader& reader, FieldType type, AdaptiveDictionary& dictionary)
{
    switch (type)
    {
    case FieldType::u8: return FieldValue::from_u8(reader.read_u8());
    case FieldType::u16: return FieldValue::from_u16(reader.read_u16());
    case FieldType::u32: return FieldValue::from_u32(reader.read_u32());
    case FieldType::i8: return FieldValue::from_i8(reader.read_i8());
    case FieldType::i16: return FieldValue::from_i16(reader.read_i16());
    case FieldType::i32: return FieldValue::from_i32(reader.read_i32());
    case FieldType::f32: return FieldValue::from_f32(reader.read_f32());
    case FieldType::f64: return FieldValue::from_f64(reader.read_f64());
    case FieldType::string:
    {
        const u16 marker_or_size = reader.read_u16();
        if (marker_or_size == kDictionaryStringReferenceMarker)
        {
            const u16 id = reader.read_u16();
            const AdaptiveDictionaryResult looked = dictionary.lookup_value(id);
            if (!looked.ok())
            {
                throw std::out_of_range("Dictionary ID lookup miss");
            }
            return FieldValue::from_string(looked.value);
        }

        std::string decoded;
        decoded.reserve(marker_or_size);
        for (u16 i = 0; i < marker_or_size; ++i)
        {
            decoded.push_back(static_cast<char>(reader.read_u8()));
        }

        (void)dictionary.create_or_get(decoded);
        return FieldValue::from_string(decoded);
    }
    case FieldType::bytes:
    {
        const u16 size = reader.read_u16();
        return FieldValue::from_bytes(reader.read_bytes(size));
    }
    }

    return FieldValue::from_u8(0);
}

bool json_value_to_field_value(
    const JsonValue& json_value,
    FieldType expected_type,
    FieldValue& out_field_value
)
{
    switch (expected_type)
    {
    case FieldType::u8:
        if (json_value.type != JsonValueType::integer
            || json_value.integer_value < 0
            || json_value.integer_value > std::numeric_limits<u8>::max())
        {
            return false;
        }
        out_field_value = FieldValue::from_u8(static_cast<u8>(json_value.integer_value));
        return true;
    case FieldType::u16:
        if (json_value.type != JsonValueType::integer
            || json_value.integer_value < 0
            || json_value.integer_value > std::numeric_limits<u16>::max())
        {
            return false;
        }
        out_field_value = FieldValue::from_u16(static_cast<u16>(json_value.integer_value));
        return true;
    case FieldType::u32:
        if (json_value.type != JsonValueType::integer
            || json_value.integer_value < 0
            || json_value.integer_value > static_cast<i64>(std::numeric_limits<u32>::max()))
        {
            return false;
        }
        out_field_value = FieldValue::from_u32(static_cast<u32>(json_value.integer_value));
        return true;
    case FieldType::i8:
        if (json_value.type != JsonValueType::integer
            || json_value.integer_value < std::numeric_limits<i8>::min()
            || json_value.integer_value > std::numeric_limits<i8>::max())
        {
            return false;
        }
        out_field_value = FieldValue::from_i8(static_cast<i8>(json_value.integer_value));
        return true;
    case FieldType::i16:
        if (json_value.type != JsonValueType::integer
            || json_value.integer_value < std::numeric_limits<i16>::min()
            || json_value.integer_value > std::numeric_limits<i16>::max())
        {
            return false;
        }
        out_field_value = FieldValue::from_i16(static_cast<i16>(json_value.integer_value));
        return true;
    case FieldType::i32:
        if (json_value.type != JsonValueType::integer
            || json_value.integer_value < std::numeric_limits<i32>::min()
            || json_value.integer_value > std::numeric_limits<i32>::max())
        {
            return false;
        }
        out_field_value = FieldValue::from_i32(static_cast<i32>(json_value.integer_value));
        return true;
    case FieldType::f32:
        if (json_value.type != JsonValueType::decimal && json_value.type != JsonValueType::integer)
        {
            return false;
        }
        out_field_value = FieldValue::from_f32(
            static_cast<f32>(json_value.type == JsonValueType::decimal ? json_value.decimal_value : json_value.integer_value)
        );
        return true;
    case FieldType::f64:
        if (json_value.type != JsonValueType::decimal && json_value.type != JsonValueType::integer)
        {
            return false;
        }
        out_field_value = FieldValue::from_f64(
            json_value.type == JsonValueType::decimal ? json_value.decimal_value : static_cast<f64>(json_value.integer_value)
        );
        return true;
    case FieldType::string:
        if (json_value.type != JsonValueType::string)
        {
            return false;
        }
        out_field_value = FieldValue::from_string(json_value.string_value);
        return true;
    case FieldType::bytes:
        if (json_value.type != JsonValueType::bytes)
        {
            return false;
        }
        out_field_value = FieldValue::from_bytes(json_value.bytes_value);
        return true;
    }

    return false;
}

JsonValue field_value_to_json_value(const FieldValue& field_value)
{
    switch (field_value.type)
    {
    case FieldType::u8: return JsonValue::from_integer(field_value.as_u8);
    case FieldType::u16: return JsonValue::from_integer(field_value.as_u16);
    case FieldType::u32: return JsonValue::from_integer(field_value.as_u32);
    case FieldType::i8: return JsonValue::from_integer(field_value.as_i8);
    case FieldType::i16: return JsonValue::from_integer(field_value.as_i16);
    case FieldType::i32: return JsonValue::from_integer(field_value.as_i32);
    case FieldType::f32: return JsonValue::from_decimal(field_value.as_f32);
    case FieldType::f64: return JsonValue::from_decimal(field_value.as_f64);
    case FieldType::string: return JsonValue::from_string(field_value.as_string);
    case FieldType::bytes: return JsonValue::from_bytes(field_value.as_bytes);
    }

    return JsonValue::from_integer(0);
}

void encode_field_value(
    Encoder& encoder,
    const FieldValue& value,
    AdaptiveDictionary& dictionary,
    const AdaptiveDictionaryConfig& dictionary_config
)
{
    switch (value.type)
    {
    case FieldType::u8: encoder.write_u8(value.as_u8); return;
    case FieldType::u16: encoder.write_u16(value.as_u16); return;
    case FieldType::u32: encoder.write_u32(value.as_u32); return;
    case FieldType::i8: encoder.write_i8(value.as_i8); return;
    case FieldType::i16: encoder.write_i16(value.as_i16); return;
    case FieldType::i32: encoder.write_i32(value.as_i32); return;
    case FieldType::f32: encoder.write_f32(value.as_f32); return;
    case FieldType::f64: encoder.write_f64(value.as_f64); return;
    case FieldType::bytes:
        encoder.write_u16(static_cast<u16>(value.as_bytes.size()));
        if (!value.as_bytes.empty())
        {
            encoder.write_bytes(value.as_bytes.data(), value.as_bytes.size());
        }
        return;
    case FieldType::string:
    {
        if (dictionary_config.enabled && dictionary_config.enable_string_values)
        {
            const AdaptiveDictionaryResult lookup = dictionary.lookup_id(value.as_string);
            if (lookup.ok())
            {
                encoder.write_u16(kDictionaryStringReferenceMarker);
                encoder.write_u16(lookup.id);
                return;
            }

            (void)dictionary.create_or_get(value.as_string);
        }

        encoder.write_string(value.as_string);
        return;
    }
    }
}

} // namespace

SchemaRegistryStatus SchemaRuntime::register_schema(const SchemaDef& schema)
{
    const SchemaValidationResult validation = SchemaValidator::validate_schema(schema, &registry_);
    if (!validation.ok())
    {
        return {SchemaRegistryStatusCode::invalid_schema, validation.message};
    }

    return registry_.register_schema(schema);
}

SchemaRuntimeResult SchemaRuntime::send(
    const std::string& schema_name,
    const NamedPayload& payload,
    Buffer& out_packet
) const
{
    const SchemaDef* schema = registry_.find_by_name(schema_name);
    if (schema == nullptr)
    {
        return {SchemaRuntimeCode::schema_not_found, "Schema not found: " + schema_name};
    }

    const SchemaValidationResult validation = SchemaValidator::validate_schema(*schema, &registry_);
    if (!validation.ok())
    {
        return {SchemaRuntimeCode::schema_invalid, validation.message};
    }

    Encoder encoder;
    encoder.write_u16(schema->message_id);

    OrderedPayload ordered_payload;
    const FieldMapperResult mapped = FieldMapper::map_named_to_ordered(*schema, payload, ordered_payload);
    if (!mapped.ok())
    {
        return {SchemaRuntimeCode::mapping_failed, mapped.message};
    }

    for (usize i = 0; i < ordered_payload.size(); ++i)
    {
        encode_field_value(encoder, ordered_payload[i], dictionary_, dictionary_.config());
    }

    Buffer encoded_packet = encoder.buffer();
    Buffer optimized_packet;
    const RuntimeOptimizerResult optimize_result =
        RuntimeOptimizer::optimize_packet(*schema, encoded_packet, optimized_packet, optimizer_config_);
    if (!optimize_result.ok)
    {
        return {SchemaRuntimeCode::decode_failed, "Runtime optimization failed."};
    }

    out_packet = optimize_result.applied ? optimized_packet : encoded_packet;
    return {SchemaRuntimeCode::ok, "Packet encoded successfully."};
}

SchemaRuntimeResult SchemaRuntime::receive(
    const Buffer& packet,
    std::string& out_schema_name,
    NamedPayload& out_payload
) const
{
    if (packet.size() < 2)
    {
        return {SchemaRuntimeCode::packet_too_small, "Packet too small to contain message_id."};
    }

    try
    {
        const u16 message_id = static_cast<u16>(packet.data()[0]) | (static_cast<u16>(packet.data()[1]) << 8);
        const SchemaDef* schema = registry_.find_by_message_id(message_id);
        if (schema == nullptr)
        {
            return {SchemaRuntimeCode::schema_not_found, "Schema not found for message_id."};
        }

        Buffer decode_packet;
        const RuntimeOptimizerResult deoptimize_result =
            RuntimeOptimizer::deoptimize_packet(*schema, packet, decode_packet);
        if (!deoptimize_result.ok)
        {
            return {SchemaRuntimeCode::decode_failed, "Runtime deoptimization failed."};
        }

        const CorruptionCheckResult corruption_check =
            CorruptionDetector::check_keydrop_packet(decode_packet, *schema);
        if (!corruption_check.ok)
        {
            return {SchemaRuntimeCode::corruption_detected, corruption_check.error_message};
        }

        PacketReader reader(decode_packet);
        (void)reader.read_u16();

        OrderedPayload ordered;
        ordered.reserve(schema->fields.size());
        for (usize i = 0; i < schema->fields.size(); ++i)
        {
            ordered.push_back(read_field_value(reader, schema->fields[i].type, dictionary_));
        }

        std::vector<FieldType> decoded_types;
        decoded_types.reserve(ordered.size());
        for (usize i = 0; i < ordered.size(); ++i)
        {
            decoded_types.push_back(ordered[i].type);
        }

        const SchemaValidationResult payload_validation =
            SchemaValidator::validate_payload_field_types(*schema, decoded_types);
        if (!payload_validation.ok())
        {
            return {SchemaRuntimeCode::decode_failed, payload_validation.message};
        }

        const FieldMapperResult mapped = FieldMapper::map_ordered_to_named(*schema, ordered, out_payload);
        if (!mapped.ok())
        {
            return {SchemaRuntimeCode::decode_failed, mapped.message};
        }

        if (!reader.empty())
        {
            return {SchemaRuntimeCode::trailing_packet_data, "Packet has trailing unread bytes."};
        }

        out_schema_name = schema->schema_name;
        return {SchemaRuntimeCode::ok, "Packet decoded successfully."};
    }
    catch (const std::out_of_range&)
    {
        return {SchemaRuntimeCode::decode_failed, "Packet ended before schema decode completed."};
    }
}

const SchemaRegistry& SchemaRuntime::registry() const
{
    return registry_;
}

void SchemaRuntime::set_optimizer_config(const RuntimeOptimizerConfig& config)
{
    optimizer_config_ = config;
}

const RuntimeOptimizerConfig& SchemaRuntime::optimizer_config() const
{
    return optimizer_config_;
}

void SchemaRuntime::set_dictionary_config(const AdaptiveDictionaryConfig& config)
{
    dictionary_.configure(config);
}

const AdaptiveDictionaryConfig& SchemaRuntime::dictionary_config() const
{
    return dictionary_.config();
}

void SchemaRuntime::reset_dictionary()
{
    dictionary_.reset();
}

SchemaRuntimeResult SchemaRuntime::send_json(
    const std::string& schema_name,
    const JsonObject& json_payload,
    Buffer& out_packet
) const
{
    const SchemaDef* schema = registry_.find_by_name(schema_name);
    if (schema == nullptr)
    {
        return {SchemaRuntimeCode::schema_not_found, "Schema not found: " + schema_name};
    }

    NamedPayload payload;
    for (usize i = 0; i < schema->fields.size(); ++i)
    {
        const FieldDef& field = schema->fields[i];
        const JsonObject::const_iterator it = json_payload.find(field.name);
        if (it == json_payload.end())
        {
            return {SchemaRuntimeCode::json_conversion_failed, "Missing required JSON field: " + field.name};
        }

        FieldValue mapped_value;
        if (!json_value_to_field_value(it->second, field.type, mapped_value))
        {
            return {SchemaRuntimeCode::json_conversion_failed, "JSON type incompatible with schema for field: " + field.name};
        }

        payload[field.name] = mapped_value;
    }

    for (JsonObject::const_iterator it = json_payload.begin(); it != json_payload.end(); ++it)
    {
        bool found = false;
        for (usize i = 0; i < schema->fields.size(); ++i)
        {
            if (schema->fields[i].name == it->first)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            return {SchemaRuntimeCode::json_conversion_failed, "Unknown extra JSON field: " + it->first};
        }
    }

    return send(schema_name, payload, out_packet);
}

SchemaRuntimeResult SchemaRuntime::receive_json(
    const Buffer& packet,
    std::string& out_schema_name,
    JsonObject& out_json_payload
) const
{
    NamedPayload payload;
    const SchemaRuntimeResult receive_result = receive(packet, out_schema_name, payload);
    if (!receive_result.ok())
    {
        return receive_result;
    }

    out_json_payload.clear();
    for (NamedPayload::const_iterator it = payload.begin(); it != payload.end(); ++it)
    {
        out_json_payload[it->first] = field_value_to_json_value(it->second);
    }

    return {SchemaRuntimeCode::ok, "JSON payload decoded successfully."};
}

SchemaRuntimeResult SchemaRuntime::send_stream(
    const std::string& schema_name,
    const NamedPayload& payload,
    Buffer& out_packet,
    bool& out_has_packet
) const
{
    Buffer base_packet;
    const SchemaRuntimeResult send_result = send(schema_name, payload, base_packet);
    if (!send_result.ok())
    {
        out_has_packet = false;
        return send_result;
    }

    StreamOptimizationOutput stream_out;
    stream_optimizer_.optimize_outgoing(schema_name, payload, base_packet, stream_out);
    out_has_packet = stream_out.emit_now;
    if (out_has_packet)
    {
        out_packet = stream_out.packet;
    }
    return {SchemaRuntimeCode::ok, "Stream packet processed."};
}

SchemaRuntimeResult SchemaRuntime::flush_stream(
    Buffer& out_packet,
    bool& out_has_packet
) const
{
    out_has_packet = stream_optimizer_.flush_batched(out_packet);
    return {SchemaRuntimeCode::ok, out_has_packet ? "Batched stream packet flushed." : "No batched packets pending."};
}

SchemaRuntimeResult SchemaRuntime::receive_stream(
    const Buffer& packet,
    std::vector<std::pair<std::string, NamedPayload>>& out_messages
) const
{
    out_messages.clear();
    std::deque<Buffer> packets;
    if (!stream_optimizer_.expand_incoming(packet, packets))
    {
        return {SchemaRuntimeCode::decode_failed, "Invalid stream packet envelope."};
    }

    while (!packets.empty())
    {
        std::string schema_name;
        NamedPayload payload;
        const SchemaRuntimeResult result = receive(packets.front(), schema_name, payload);
        if (!result.ok())
        {
            return result;
        }

        out_messages.push_back(std::make_pair(schema_name, payload));
        packets.pop_front();
    }

    return {SchemaRuntimeCode::ok, "Stream packet decoded."};
}

void SchemaRuntime::set_stream_optimizer_config(const StreamOptimizerConfig& config)
{
    stream_optimizer_.configure(config);
}

const StreamOptimizerConfig& SchemaRuntime::stream_optimizer_config() const
{
    return stream_optimizer_.config();
}

void SchemaRuntime::reset_stream_optimizer()
{
    stream_optimizer_.reset();
}

}
