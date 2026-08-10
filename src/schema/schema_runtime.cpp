#include "keydrop/schema/schema_runtime.hpp"

#include <array>
#include <deque>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include "keydrop/core/encoder.hpp"
#include "keydrop/core/packet_reader.hpp"
#include "keydrop/reliability/corruption_detector.hpp"
#include "keydrop/reliability/packet_synchronizer.hpp"

namespace keydrop {

namespace {
constexpr u16 kDictionaryStringReferenceMarker = 0xFFFF;

using DecodeFieldFn = FieldValue (*)(PacketReader&, AdaptiveDictionary&);
using EncodeFieldFn = void (*)(
    Encoder&,
    const FieldValue&,
    AdaptiveDictionary&,
    const AdaptiveDictionaryConfig&
);

FieldValue decode_u8(PacketReader& reader, AdaptiveDictionary&)
{
    return FieldValue::from_u8(reader.read_u8());
}

FieldValue decode_u16(PacketReader& reader, AdaptiveDictionary&)
{
    return FieldValue::from_u16(reader.read_u16());
}

FieldValue decode_u32(PacketReader& reader, AdaptiveDictionary&)
{
    return FieldValue::from_u32(reader.read_u32());
}

FieldValue decode_i8(PacketReader& reader, AdaptiveDictionary&)
{
    return FieldValue::from_i8(reader.read_i8());
}

FieldValue decode_i16(PacketReader& reader, AdaptiveDictionary&)
{
    return FieldValue::from_i16(reader.read_i16());
}

FieldValue decode_i32(PacketReader& reader, AdaptiveDictionary&)
{
    return FieldValue::from_i32(reader.read_i32());
}

FieldValue decode_f32(PacketReader& reader, AdaptiveDictionary&)
{
    return FieldValue::from_f32(reader.read_f32());
}

FieldValue decode_f64(PacketReader& reader, AdaptiveDictionary&)
{
    return FieldValue::from_f64(reader.read_f64());
}

FieldValue decode_string(PacketReader& reader, AdaptiveDictionary& dictionary)
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

    const std::string decoded = reader.read_string_from_size(marker_or_size);
    if (dictionary.config().enabled && dictionary.config().enable_string_values)
    {
        (void)dictionary.create_or_get(decoded);
    }
    return FieldValue::from_string(decoded);
}

FieldValue decode_bytes(PacketReader& reader, AdaptiveDictionary&)
{
    const u16 size = reader.read_u16();
    return FieldValue::from_bytes(reader.read_bytes(size));
}

void encode_u8(Encoder& encoder, const FieldValue& value, AdaptiveDictionary&, const AdaptiveDictionaryConfig&)
{
    encoder.write_u8(value.as_u8);
}

void encode_u16(Encoder& encoder, const FieldValue& value, AdaptiveDictionary&, const AdaptiveDictionaryConfig&)
{
    encoder.write_u16(value.as_u16);
}

void encode_u32(Encoder& encoder, const FieldValue& value, AdaptiveDictionary&, const AdaptiveDictionaryConfig&)
{
    encoder.write_u32(value.as_u32);
}

void encode_i8(Encoder& encoder, const FieldValue& value, AdaptiveDictionary&, const AdaptiveDictionaryConfig&)
{
    encoder.write_i8(value.as_i8);
}

void encode_i16(Encoder& encoder, const FieldValue& value, AdaptiveDictionary&, const AdaptiveDictionaryConfig&)
{
    encoder.write_i16(value.as_i16);
}

void encode_i32(Encoder& encoder, const FieldValue& value, AdaptiveDictionary&, const AdaptiveDictionaryConfig&)
{
    encoder.write_i32(value.as_i32);
}

void encode_f32(Encoder& encoder, const FieldValue& value, AdaptiveDictionary&, const AdaptiveDictionaryConfig&)
{
    encoder.write_f32(value.as_f32);
}

void encode_f64(Encoder& encoder, const FieldValue& value, AdaptiveDictionary&, const AdaptiveDictionaryConfig&)
{
    encoder.write_f64(value.as_f64);
}

void encode_bytes(Encoder& encoder, const FieldValue& value, AdaptiveDictionary&, const AdaptiveDictionaryConfig&)
{
    encoder.write_u16(static_cast<u16>(value.as_bytes.size()));
    if (!value.as_bytes.empty())
    {
        encoder.write_bytes(value.as_bytes.data(), value.as_bytes.size());
    }
}

void encode_string(
    Encoder& encoder,
    const FieldValue& value,
    AdaptiveDictionary& dictionary,
    const AdaptiveDictionaryConfig& dictionary_config
)
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
}

const std::array<DecodeFieldFn, static_cast<usize>(FieldCodec::count)> kDecodeField = {{
    decode_u8,
    decode_u16,
    decode_u32,
    decode_i8,
    decode_i16,
    decode_i32,
    decode_f32,
    decode_f64,
    decode_string,
    decode_bytes,
}};

const std::array<EncodeFieldFn, static_cast<usize>(FieldCodec::count)> kEncodeField = {{
    encode_u8,
    encode_u16,
    encode_u32,
    encode_i8,
    encode_i16,
    encode_i32,
    encode_f32,
    encode_f64,
    encode_string,
    encode_bytes,
}};

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

const char* json_value_type_to_string(JsonValueType type)
{
    switch (type)
    {
    case JsonValueType::integer: return "integer";
    case JsonValueType::decimal: return "decimal";
    case JsonValueType::string: return "string";
    case JsonValueType::bytes: return "hex bytes";
    }
    return "unknown";
}

std::string json_value_display_string(const JsonValue& value)
{
    switch (value.type)
    {
    case JsonValueType::integer: return std::to_string(value.integer_value);
    case JsonValueType::decimal: return std::to_string(value.decimal_value);
    case JsonValueType::string: return "\"" + value.string_value + "\"";
    case JsonValueType::bytes: return "\"0x bytes\"";
    }
    return "unknown";
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

usize estimate_encoded_packet_size(
    const PacketLayout& layout,
    const OrderedPayload& ordered_payload
)
{
    usize size = 2;
    for (usize i = 0; i < layout.fields.size(); ++i)
    {
        const FieldLayout& field_layout = layout.fields[i];
        const FieldValue& value = ordered_payload[field_layout.schema_index];
        if (!field_layout.variable_length)
        {
            size += field_layout.fixed_size;
            continue;
        }

        size += 2;
        if (value.type == FieldType::string)
        {
            size += value.as_string.size();
        }
        else if (value.type == FieldType::bytes)
        {
            size += value.as_bytes.size();
        }
    }
    return size;
}

SchemaRuntimeResult encode_ordered_with_schema(
    const SchemaDef& schema,
    const PacketLayout& layout,
    const OrderedPayload& ordered_payload,
    const RuntimeOptimizerConfig& optimizer_config,
    AdaptiveDictionary& dictionary,
    BufferPool& buffer_pool,
    Buffer& out_packet
)
{
    const SchemaValidationResult payload_validation =
        SchemaValidator::validate_payload_values(schema, ordered_payload);
    if (!payload_validation.ok())
    {
        return {SchemaRuntimeCode::schema_mismatch, payload_validation.message};
    }

    Encoder encoder;
    encoder.reserve(estimate_encoded_packet_size(layout, ordered_payload));
    encoder.write_u16(schema.message_id);

    const auto& dict_cfg = dictionary.config();
    for (usize i = 0; i < layout.fields.size(); ++i)
    {
        const FieldLayout& field_layout = layout.fields[i];
        const FieldValue& value = ordered_payload[field_layout.schema_index];
        switch (field_layout.codec)
        {
        case FieldCodec::u8_value:  encoder.write_u8(value.as_u8); break;
        case FieldCodec::u16_value: encoder.write_u16(value.as_u16); break;
        case FieldCodec::u32_value: encoder.write_u32(value.as_u32); break;
        case FieldCodec::i8_value:  encoder.write_i8(value.as_i8); break;
        case FieldCodec::i16_value: encoder.write_i16(value.as_i16); break;
        case FieldCodec::i32_value: encoder.write_i32(value.as_i32); break;
        case FieldCodec::f32_value: encoder.write_f32(value.as_f32); break;
        case FieldCodec::f64_value: encoder.write_f64(value.as_f64); break;
        case FieldCodec::string_value:
            encode_string(encoder, value, dictionary, dict_cfg);
            break;
        case FieldCodec::bytes_value:
            encode_bytes(encoder, value, dictionary, dict_cfg);
            break;
        case FieldCodec::count: break;
        }
    }

    if (optimizer_config.enabled && optimizer_config.enable_zero_value_omission)
    {
        Buffer encoded_packet = encoder.take_buffer();
        BufferLease optimized_packet_lease = buffer_pool.lease();
        Buffer& optimized_packet = optimized_packet_lease.get();
        const RuntimeOptimizerResult optimize_result =
            RuntimeOptimizer::optimize_packet(schema, encoded_packet, optimized_packet, optimizer_config);
        if (!optimize_result.ok)
        {
            return {SchemaRuntimeCode::decode_failed, "Runtime optimization failed."};
        }

        if (optimize_result.applied)
        {
            out_packet = std::move(optimized_packet);
        }
        else
        {
            out_packet = std::move(encoded_packet);
        }
    }
    else
    {
        out_packet = encoder.take_buffer();
    }
    return {SchemaRuntimeCode::ok, "Packet encoded successfully."};
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
    const SchemaDef* schema = nullptr;
    const PacketLayout* layout = nullptr;

    if (schema_name == cached_schema_name_ && cached_schema_ != nullptr)
    {
        schema = cached_schema_;
        layout = cached_layout_;
    }
    else
    {
        schema = registry_.find_by_name(schema_name);
        if (schema == nullptr)
        {
            return {SchemaRuntimeCode::schema_not_found, "Schema not found: " + schema_name};
        }

        layout = registry_.find_layout_by_name(schema_name);
        if (layout == nullptr)
        {
            return {SchemaRuntimeCode::schema_invalid, "Packet layout not found for schema."};
        }

        cached_schema_name_ = schema_name;
        cached_schema_ = schema;
        cached_layout_ = layout;
    }

    OrderedPayloadLease ordered_payload_lease =
        payload_pool_.lease_ordered(schema->fields.size());
    OrderedPayload& ordered_payload = ordered_payload_lease.get();
    const FieldMapperResult mapped = FieldMapper::map_named_to_ordered(*schema, payload, ordered_payload);
    if (!mapped.ok())
    {
        return {SchemaRuntimeCode::mapping_failed, mapped.message};
    }

    const SchemaRuntimeResult result = encode_ordered_with_schema(
        *schema,
        *layout,
        ordered_payload,
        optimizer_config_,
        dictionary_,
        buffer_pool_,
        out_packet
    );

    if (result.ok())
    {
        adaptive_profiler_.observe(schema_name, payload);
        adaptive_profiler_.maybe_apply(*this);
    }

    return result;
}

SchemaRuntimeResult SchemaRuntime::send_ordered(
    const std::string& schema_name,
    const OrderedPayload& payload,
    Buffer& out_packet
) const
{
    const SchemaDef* schema = nullptr;
    const PacketLayout* layout = nullptr;

    if (schema_name == cached_schema_name_ && cached_schema_ != nullptr)
    {
        schema = cached_schema_;
        layout = cached_layout_;
    }
    else
    {
        schema = registry_.find_by_name(schema_name);
        if (schema == nullptr)
        {
            return {SchemaRuntimeCode::schema_not_found, "Schema not found: " + schema_name};
        }

        layout = registry_.find_layout_by_name(schema_name);
        if (layout == nullptr)
        {
            return {SchemaRuntimeCode::schema_invalid, "Packet layout not found for schema."};
        }

        cached_schema_name_ = schema_name;
        cached_schema_ = schema;
        cached_layout_ = layout;
    }

    return encode_ordered_with_schema(
        *schema,
        *layout,
        payload,
        optimizer_config_,
        dictionary_,
        buffer_pool_,
        out_packet
    );
}

SchemaRuntimeResult SchemaRuntime::fast_encode(
    const std::string& schema_name,
    const FieldValue* values,
    usize count,
    Buffer& out_packet
) const
{
    const FastCodec* codec = registry_.find_fast_codec_by_name(schema_name);
    if (codec == nullptr)
    {
        return {SchemaRuntimeCode::schema_not_found, "Schema not found: " + schema_name};
    }

    const FastCodecResult result = codec->encode(values, count, out_packet);
    if (!result.ok())
    {
        return {SchemaRuntimeCode::mapping_failed, result.message};
    }

    return {SchemaRuntimeCode::ok, ""};
}

SchemaRuntimeResult SchemaRuntime::fast_decode(
    const Buffer& packet,
    std::string& out_schema_name,
    FastDecodedField* out_fields,
    usize max_fields,
    usize& out_count
) const
{
    out_count = 0;
    if (packet.size() < 2)
    {
        return {SchemaRuntimeCode::packet_too_small, "Packet too small to contain message_id."};
    }

    if (packet.data()[0] == 0xFC)
    {
        return {SchemaRuntimeCode::decode_failed, "Stream batch envelope; use receive_stream()."};
    }

    const u16 message_id = static_cast<u16>(packet.data()[0]) | (static_cast<u16>(packet.data()[1]) << 8);
    const FastCodec* codec = registry_.find_fast_codec_by_message_id(message_id);
    if (codec == nullptr)
    {
        return {SchemaRuntimeCode::schema_not_found, "Schema not found for message_id."};
    }

    usize count = 0;
    const FastCodecResult result = codec->decode(packet, out_fields, max_fields, count, dictionary_);
    if (!result.ok())
    {
        const SchemaRuntimeCode code =
            result.code == FastCodecCode::message_id_mismatch
            ? SchemaRuntimeCode::schema_mismatch
            : SchemaRuntimeCode::decode_failed;
        return {code, result.message};
    }

    out_schema_name = codec->schema().schema_name;
    out_count = count;
    return {SchemaRuntimeCode::ok, ""};
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

        const SchemaValidationResult message_validation =
            SchemaValidator::validate_message_id(*schema, message_id);
        if (!message_validation.ok())
        {
            return {SchemaRuntimeCode::schema_mismatch, message_validation.message};
        }

        return receive_with_schema(*schema, packet, out_schema_name, out_payload);
    }
    catch (const std::out_of_range&)
    {
        return {SchemaRuntimeCode::decode_failed, "Packet ended before schema decode completed."};
    }
}

SchemaRuntimeResult SchemaRuntime::receive_ordered(
    const Buffer& packet,
    std::string& out_schema_name,
    OrderedPayload& out_payload
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

        const PacketLayout* layout =
            registry_.find_layout_by_message_id(schema->message_id);
        if (layout == nullptr)
        {
            return {SchemaRuntimeCode::schema_invalid, "Packet layout not found for schema."};
        }

        // Only deoptimize if packet is actually optimized
        const Buffer* decode_packet = &packet;
        std::unique_ptr<BufferLease> decode_lease;

        if (optimizer_config_.enabled && RuntimeOptimizer::is_optimized_packet(packet))
        {
            decode_lease.reset(new BufferLease(buffer_pool_.lease()));
            Buffer& temp = decode_lease->get();
            const RuntimeOptimizerResult deoptimize_result =
                RuntimeOptimizer::deoptimize_packet(*schema, packet, temp);
            if (!deoptimize_result.ok)
            {
                return {SchemaRuntimeCode::decode_failed, "Runtime deoptimization failed."};
            }
            decode_packet = &temp;
        }

        PacketReader reader(*decode_packet);
        (void)reader.read_u16();

        out_payload.clear();
        out_payload.reserve(layout->fields.size());
        for (usize i = 0; i < layout->fields.size(); ++i)
        {
            switch (layout->fields[i].codec)
            {
            case FieldCodec::u8_value:  out_payload.push_back(FieldValue::from_u8(reader.read_u8())); break;
            case FieldCodec::u16_value: out_payload.push_back(FieldValue::from_u16(reader.read_u16())); break;
            case FieldCodec::u32_value: out_payload.push_back(FieldValue::from_u32(reader.read_u32())); break;
            case FieldCodec::i8_value:  out_payload.push_back(FieldValue::from_i8(reader.read_i8())); break;
            case FieldCodec::i16_value: out_payload.push_back(FieldValue::from_i16(reader.read_i16())); break;
            case FieldCodec::i32_value: out_payload.push_back(FieldValue::from_i32(reader.read_i32())); break;
            case FieldCodec::f32_value: out_payload.push_back(FieldValue::from_f32(reader.read_f32())); break;
            case FieldCodec::f64_value: out_payload.push_back(FieldValue::from_f64(reader.read_f64())); break;
            case FieldCodec::string_value: out_payload.push_back(decode_string(reader, dictionary_)); break;
            case FieldCodec::bytes_value:  out_payload.push_back(decode_bytes(reader, dictionary_)); break;
            case FieldCodec::count: break;
            }
        }

        out_schema_name = schema->schema_name;
        return {SchemaRuntimeCode::ok, "Packet decoded successfully."};
    }
    catch (const std::out_of_range&)
    {
        return {SchemaRuntimeCode::decode_failed, "Packet ended before schema decode completed."};
    }
}

SchemaRuntimeResult SchemaRuntime::receive_as(
    const std::string& expected_schema_name,
    const Buffer& packet,
    NamedPayload& out_payload
) const
{
    if (packet.size() < 2)
    {
        return {SchemaRuntimeCode::packet_too_small, "Packet too small to contain message_id."};
    }

    const SchemaDef* schema = registry_.find_by_name(expected_schema_name);
    if (schema == nullptr)
    {
        return {SchemaRuntimeCode::schema_not_found, "Schema not found: " + expected_schema_name};
    }

    const u16 message_id =
        static_cast<u16>(packet.data()[0])
        |
        (static_cast<u16>(packet.data()[1]) << 8);
    const SchemaValidationResult message_validation =
        SchemaValidator::validate_message_id(*schema, message_id);
    if (!message_validation.ok())
    {
        return {SchemaRuntimeCode::schema_mismatch, message_validation.message};
    }

    std::string decoded_schema_name;
    return receive_with_schema(*schema, packet, decoded_schema_name, out_payload);
}

SchemaRuntimeResult SchemaRuntime::receive_with_schema(
    const SchemaDef& schema,
    const Buffer& packet,
    std::string& out_schema_name,
    NamedPayload& out_payload
) const
{
    try
    {
        // Only lease + deoptimize if the packet is actually optimized
        const Buffer* decode_packet = &packet;
        std::unique_ptr<BufferLease> decode_lease;

        if (optimizer_config_.enabled && RuntimeOptimizer::is_optimized_packet(packet))
        {
            decode_lease.reset(new BufferLease(buffer_pool_.lease()));
            Buffer& temp = decode_lease->get();
            const RuntimeOptimizerResult deoptimize_result =
                RuntimeOptimizer::deoptimize_packet(schema, packet, temp);
            if (!deoptimize_result.ok)
            {
                return {SchemaRuntimeCode::decode_failed, "Runtime deoptimization failed."};
            }
            decode_packet = &temp;
        }

        // Use schema message_id for layout lookup (avoids second name hash)
        const PacketLayout* layout =
            registry_.find_layout_by_message_id(schema.message_id);
        if (layout == nullptr)
        {
            return {SchemaRuntimeCode::schema_invalid, "Packet layout not found for schema."};
        }

        const CorruptionCheckResult corruption_check =
            CorruptionDetector::check_keydrop_packet(
                *decode_packet,
                *layout
            );
        if (!corruption_check.ok)
        {
            return {SchemaRuntimeCode::corruption_detected, corruption_check.error_message};
        }

        PacketReader reader(*decode_packet);
        (void)reader.read_u16();

        OrderedPayloadLease ordered_lease =
            payload_pool_.lease_ordered(layout->fields.size());
        OrderedPayload& ordered = ordered_lease.get();
        ordered.reserve(layout->fields.size());
        for (usize i = 0; i < layout->fields.size(); ++i)
        {
            switch (layout->fields[i].codec)
            {
            case FieldCodec::u8_value:  ordered.push_back(FieldValue::from_u8(reader.read_u8())); break;
            case FieldCodec::u16_value: ordered.push_back(FieldValue::from_u16(reader.read_u16())); break;
            case FieldCodec::u32_value: ordered.push_back(FieldValue::from_u32(reader.read_u32())); break;
            case FieldCodec::i8_value:  ordered.push_back(FieldValue::from_i8(reader.read_i8())); break;
            case FieldCodec::i16_value: ordered.push_back(FieldValue::from_i16(reader.read_i16())); break;
            case FieldCodec::i32_value: ordered.push_back(FieldValue::from_i32(reader.read_i32())); break;
            case FieldCodec::f32_value: ordered.push_back(FieldValue::from_f32(reader.read_f32())); break;
            case FieldCodec::f64_value: ordered.push_back(FieldValue::from_f64(reader.read_f64())); break;
            case FieldCodec::string_value: ordered.push_back(decode_string(reader, dictionary_)); break;
            case FieldCodec::bytes_value:  ordered.push_back(decode_bytes(reader, dictionary_)); break;
            case FieldCodec::count: break;
            }
        }

        const SchemaValidationResult payload_validation =
            SchemaValidator::validate_payload_values(schema, ordered);
        if (!payload_validation.ok())
        {
            return {SchemaRuntimeCode::schema_mismatch, payload_validation.message};
        }

        const FieldMapperResult mapped = FieldMapper::map_ordered_to_named(schema, ordered, out_payload);
        if (!mapped.ok())
        {
            return {SchemaRuntimeCode::schema_mismatch, mapped.message};
        }

        if (!reader.empty())
        {
            return {SchemaRuntimeCode::corruption_detected, "Packet has trailing unread bytes."};
        }

        out_schema_name = schema.schema_name;
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
    optimizer_explicit_ = true;
}

const RuntimeOptimizerConfig& SchemaRuntime::optimizer_config() const
{
    return optimizer_config_;
}

void SchemaRuntime::set_dictionary_config(const AdaptiveDictionaryConfig& config)
{
    dictionary_.configure(config);
    dictionary_explicit_ = true;
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

    NamedPayloadLease payload_lease =
        payload_pool_.lease_named(schema->fields.size());
    NamedPayload& payload = payload_lease.get();
    for (usize i = 0; i < schema->fields.size(); ++i)
    {
        const FieldDef& field = schema->fields[i];
        const JsonObject::const_iterator it = json_payload.find(field.name);
        if (it == json_payload.end())
        {
            return {SchemaRuntimeCode::json_conversion_failed, "Missing required JSON field '" + field.name + "'. Add the field or update the schema."};
        }

        FieldValue mapped_value;
        if (!json_value_to_field_value(it->second, field.type, mapped_value))
        {
            return {
                SchemaRuntimeCode::json_conversion_failed,
                "Field '" + field.name + "' expects " + field_type_to_string(field.type)
                    + " but received " + json_value_type_to_string(it->second.type)
                    + " with value " + json_value_display_string(it->second)
                    + ". Correct the JSON value or update the schema."
            };
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
            return {SchemaRuntimeCode::json_conversion_failed, "Unknown extra JSON field '" + it->first + "'. Remove it or update the schema."};
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
    NamedPayloadLease payload_lease = payload_pool_.lease_named(0);
    NamedPayload& payload = payload_lease.get();
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

    const SchemaDef* schema = registry_.find_by_name(schema_name);
    if (schema == nullptr)
    {
        out_has_packet = false;
        return {SchemaRuntimeCode::schema_not_found, "Schema not found: " + schema_name};
    }

    StreamOptimizationOutput stream_out;
    stream_optimizer_.optimize_outgoing(*schema, schema_name, payload, base_packet, stream_out);
    out_has_packet = stream_out.emit_now;
    if (out_has_packet)
    {
        out_packet = stream_out.packet;
        if (reliability_config_.enable_crc32)
        {
            Buffer wrapped;
            wrapped.write(kCrcWrapperMarker);
            const u32 crc = CorruptionDetector::crc32(
                out_packet.data().data(),
                out_packet.size()
            );
            wrapped.write(static_cast<byte>(crc & 0xFF));
            wrapped.write(static_cast<byte>((crc >> 8) & 0xFF));
            wrapped.write(static_cast<byte>((crc >> 16) & 0xFF));
            wrapped.write(static_cast<byte>((crc >> 24) & 0xFF));
            wrapped.append(out_packet);
            out_packet = wrapped;
        }
    }
    return {SchemaRuntimeCode::ok, "Stream packet processed."};
}

SchemaRuntimeResult SchemaRuntime::flush_stream(
    Buffer& out_packet,
    bool& out_has_packet
) const
{
    out_has_packet = stream_optimizer_.flush_batched(out_packet);
    if (out_has_packet && reliability_config_.enable_crc32)
    {
        Buffer wrapped;
        wrapped.write(kCrcWrapperMarker);
        const u32 crc = CorruptionDetector::crc32(
            out_packet.data().data(),
            out_packet.size()
        );
        wrapped.write(static_cast<byte>(crc & 0xFF));
        wrapped.write(static_cast<byte>((crc >> 8) & 0xFF));
        wrapped.write(static_cast<byte>((crc >> 16) & 0xFF));
        wrapped.write(static_cast<byte>((crc >> 24) & 0xFF));
        wrapped.append(out_packet);
        out_packet = wrapped;
    }
    return {SchemaRuntimeCode::ok, out_has_packet ? "Batched stream packet flushed." : "No batched packets pending."};
}

SchemaRuntimeResult SchemaRuntime::receive_stream(
    const Buffer& packet,
    std::vector<std::pair<std::string, NamedPayload>>& out_messages
) const
{
    out_messages.clear();
    if (packet.empty())
    {
        return {SchemaRuntimeCode::decode_failed, "Empty stream packet."};
    }

    // Phase 5: CRC32 envelope. Verify and unwrap before any dispatch so a
    // corrupted stream packet is rejected, never decoded.
    const Buffer* working = &packet;
    Buffer unwrapped;
    if (packet.data()[0] == kCrcWrapperMarker)
    {
        if (packet.size() < 5)
        {
            return {SchemaRuntimeCode::corruption_detected, "CRC wrapper too small."};
        }

        const u32 stored_crc =
            static_cast<u32>(packet.data()[1])
            | (static_cast<u32>(packet.data()[2]) << 8)
            | (static_cast<u32>(packet.data()[3]) << 16)
            | (static_cast<u32>(packet.data()[4]) << 24);
        const u32 actual_crc = CorruptionDetector::crc32(
            packet.data().data() + 5,
            packet.size() - 5
        );
        if (stored_crc != actual_crc)
        {
            return {SchemaRuntimeCode::corruption_detected, "Stream packet CRC32 mismatch."};
        }

        unwrapped.append(packet.data().data() + 5, packet.size() - 5);
        working = &unwrapped;
    }

    // Control packet: dictionary reset (Phase 3A). No payload follows.
    if (working->data()[0] == StreamOptimizer::kControlMarker)
    {
        dictionary_.reset();
        return {SchemaRuntimeCode::ok, "Dictionary reset applied."};
    }

    // Stateful delta packet (Phase 3B/3C): expand against the last decoded
    // payload for the schema, then decode the rebuilt full packet.
    if (working->data()[0] == StreamOptimizer::kDeltaMarker)
    {
        if (working->size() < 6)
        {
            return {SchemaRuntimeCode::decode_failed, "Delta packet too small."};
        }

        const u16 message_id = static_cast<u16>(working->data()[1]) | (static_cast<u16>(working->data()[2]) << 8);
        const SchemaDef* schema = registry_.find_by_message_id(message_id);
        if (schema == nullptr)
        {
            return {SchemaRuntimeCode::schema_not_found, "Schema not found for delta packet message_id."};
        }

        Buffer full_packet;
        NamedPayload merged_payload;
        if (!stream_optimizer_.expand_delta(*schema, *working, full_packet, merged_payload))
        {
            return {
                SchemaRuntimeCode::decode_failed,
                "Delta packet rejected (sequence mismatch or missing keyframe). Wait for the next full packet."
            };
        }

        std::string schema_name;
        NamedPayloadLease payload_lease = payload_pool_.lease_named(0);
        NamedPayload& payload = payload_lease.get();
        const SchemaRuntimeResult result = receive(full_packet, schema_name, payload);
        if (!result.ok())
        {
            return result;
        }

        stream_optimizer_.record_decoded_delta(schema_name, payload);
        out_messages.push_back(std::make_pair(schema_name, payload));
        return {SchemaRuntimeCode::ok, "Delta stream packet decoded."};
    }

    std::deque<Buffer> packets;
    if (!stream_optimizer_.expand_incoming(*working, packets))
    {
        return {SchemaRuntimeCode::decode_failed, "Invalid stream packet envelope."};
    }

    while (!packets.empty())
    {
        std::string schema_name;
        NamedPayloadLease payload_lease = payload_pool_.lease_named(0);
        NamedPayload& payload = payload_lease.get();
        const SchemaRuntimeResult result = receive(packets.front(), schema_name, payload);
        if (!result.ok())
        {
            return result;
        }

        stream_optimizer_.record_decoded(schema_name, payload);
        out_messages.push_back(std::make_pair(schema_name, payload));
        packets.pop_front();
    }

    return {SchemaRuntimeCode::ok, "Stream packet decoded."};
}

SchemaRuntimeResult SchemaRuntime::receive_recovered_stream(
    const Buffer& stream,
    std::vector<std::pair<std::string, NamedPayload>>& out_messages,
    usize& out_skipped_bytes
) const
{
    out_messages.clear();
    out_skipped_bytes = 0;

    std::vector<PacketSyncResult> recovered_packets;
    if (!PacketSynchronizer::recover_all_packets(stream, registry_, recovered_packets))
    {
        return {SchemaRuntimeCode::synchronization_failed, "No recoverable packet found in stream."};
    }

    // Decoder memory limit: never decode more than the configured cap.
    const usize limit = reliability_config_.max_recovered_packets;
    const usize process_count =
        recovered_packets.size() < limit ? recovered_packets.size() : limit;

    for (usize i = 0; i < process_count; ++i)
    {
        const PacketSyncResult& recovered = recovered_packets[i];
        out_skipped_bytes += recovered.skipped_bytes;

        std::string schema_name;
        NamedPayloadLease payload_lease = payload_pool_.lease_named(0);
        NamedPayload& payload = payload_lease.get();
        const SchemaRuntimeResult receive_result =
            receive(recovered.packet, schema_name, payload);
        if (!receive_result.ok())
        {
            return receive_result;
        }

        out_messages.push_back(std::make_pair(schema_name, payload));
    }

    return {SchemaRuntimeCode::ok, "Recovered synchronized stream packets."};
}

SchemaRuntimeResult SchemaRuntime::send_dictionary_reset(Buffer& out_packet) const
{
    out_packet.clear();
    out_packet.write(StreamOptimizer::kControlMarker);
    out_packet.write(0x00);
    return {SchemaRuntimeCode::ok, ""};
}

void SchemaRuntime::set_stream_optimizer_config(const StreamOptimizerConfig& config)
{
    stream_optimizer_.configure(config);
    stream_explicit_ = true;
}

const StreamOptimizerConfig& SchemaRuntime::stream_optimizer_config() const
{
    return stream_optimizer_.config();
}

void SchemaRuntime::set_adaptive_config(const AdaptiveProfilerConfig& config)
{
    adaptive_profiler_.configure(config);
}

const AdaptiveProfilerConfig& SchemaRuntime::adaptive_config() const
{
    return adaptive_profiler_.config();
}

void SchemaRuntime::reset_adaptive_profiler()
{
    adaptive_profiler_.reset();
}

void SchemaRuntime::set_reliability_config(const ReliabilityConfig& config)
{
    reliability_config_ = config;
    if (reliability_config_.max_recovered_packets == 0)
    {
        reliability_config_.max_recovered_packets = 1;
    }
}

const ReliabilityConfig& SchemaRuntime::reliability_config() const
{
    return reliability_config_;
}

bool SchemaRuntime::dictionary_explicit() const
{
    return dictionary_explicit_;
}

bool SchemaRuntime::optimizer_explicit() const
{
    return optimizer_explicit_;
}

bool SchemaRuntime::stream_explicit() const
{
    return stream_explicit_;
}

void SchemaRuntime::apply_optimization_settings(
    const AdaptiveDictionaryConfig& dictionary,
    const RuntimeOptimizerConfig& optimizer,
    const StreamOptimizerConfig& stream
) const
{
    const bool delta_toggled = stream_optimizer_.config().enable_delta_packets != stream.enable_delta_packets;
    dictionary_.configure(dictionary);
    optimizer_config_ = optimizer;
    stream_optimizer_.configure(stream);
    if (delta_toggled)
    {
        // Force a keyframe on the next emission (queued batches are kept).
        // This closes the stale-base window where a delta could otherwise
        // be accepted after packets were dropped while delta mode was off.
        stream_optimizer_.reset_delta_state();
    }
}

void SchemaRuntime::reset_stream_optimizer()
{
    stream_optimizer_.reset();
}

void SchemaRuntime::set_buffer_pool_config(const BufferPoolConfig& config)
{
    buffer_pool_.configure(config);
}

const BufferPoolConfig& SchemaRuntime::buffer_pool_config() const
{
    return buffer_pool_.config();
}

void SchemaRuntime::set_payload_pool_config(const PayloadPoolConfig& config)
{
    payload_pool_.configure(config);
}

const PayloadPoolConfig& SchemaRuntime::payload_pool_config() const
{
    return payload_pool_.config();
}

void SchemaRuntime::reset_memory_pools()
{
    buffer_pool_.reset();
    payload_pool_.reset();
}

}
