#include "keydrop/schema/field_mapper.hpp"

#include <sstream>

namespace keydrop {

namespace {

bool field_value_matches_type(const FieldValue& value, FieldType expected_type)
{
    return value.type == expected_type;
}

void write_value(Encoder& encoder, const FieldValue& value)
{
    switch (value.type)
    {
    case FieldType::u8: encoder.write_u8(value.as_u8); break;
    case FieldType::u16: encoder.write_u16(value.as_u16); break;
    case FieldType::u32: encoder.write_u32(value.as_u32); break;
    case FieldType::i8: encoder.write_i8(value.as_i8); break;
    case FieldType::i16: encoder.write_i16(value.as_i16); break;
    case FieldType::i32: encoder.write_i32(value.as_i32); break;
    case FieldType::f32: encoder.write_f32(value.as_f32); break;
    case FieldType::f64: encoder.write_f64(value.as_f64); break;
    case FieldType::string: encoder.write_string(value.as_string); break;
    case FieldType::bytes:
        encoder.write_u16(static_cast<u16>(value.as_bytes.size()));
        if (!value.as_bytes.empty())
        {
            encoder.write_bytes(value.as_bytes.data(), value.as_bytes.size());
        }
        break;
    }
}

} // namespace

FieldValue FieldValue::from_u8(u8 value) { FieldValue out; out.type = FieldType::u8; out.as_u8 = value; return out; }
FieldValue FieldValue::from_u16(u16 value) { FieldValue out; out.type = FieldType::u16; out.as_u16 = value; return out; }
FieldValue FieldValue::from_u32(u32 value) { FieldValue out; out.type = FieldType::u32; out.as_u32 = value; return out; }
FieldValue FieldValue::from_i8(i8 value) { FieldValue out; out.type = FieldType::i8; out.as_i8 = value; return out; }
FieldValue FieldValue::from_i16(i16 value) { FieldValue out; out.type = FieldType::i16; out.as_i16 = value; return out; }
FieldValue FieldValue::from_i32(i32 value) { FieldValue out; out.type = FieldType::i32; out.as_i32 = value; return out; }
FieldValue FieldValue::from_f32(f32 value) { FieldValue out; out.type = FieldType::f32; out.as_f32 = value; return out; }
FieldValue FieldValue::from_f64(f64 value) { FieldValue out; out.type = FieldType::f64; out.as_f64 = value; return out; }
FieldValue FieldValue::from_string(const std::string& value) { FieldValue out; out.type = FieldType::string; out.as_string = value; return out; }
FieldValue FieldValue::from_bytes(const std::vector<byte>& value) { FieldValue out; out.type = FieldType::bytes; out.as_bytes = value; return out; }

FieldMapperResult FieldMapper::map_named_to_ordered(
    const SchemaDef& schema,
    const NamedPayload& named_payload,
    OrderedPayload& out_ordered_payload
)
{
    out_ordered_payload.clear();
    out_ordered_payload.resize(schema.fields.size());

    for (usize i = 0; i < schema.fields.size(); ++i)
    {
        const FieldDef& field = schema.fields[i];
        const auto payload_it = named_payload.find(field.name);
        if (payload_it == named_payload.end())
        {
            return {FieldMapperCode::missing_required_field, "Missing required field: " + field.name};
        }

        if (!field_value_matches_type(payload_it->second, field.type))
        {
            std::ostringstream oss;
            oss << "Field type mismatch for '" << field.name << "'. Expected "
                << field_type_to_string(field.type) << ", got "
                << field_type_to_string(payload_it->second.type) << ".";
            return {FieldMapperCode::field_type_mismatch, oss.str()};
        }

        out_ordered_payload[i] = payload_it->second;
    }

    if (named_payload.size() > schema.fields.size())
    {
        for (NamedPayload::const_iterator it = named_payload.begin(); it != named_payload.end(); ++it)
        {
            bool known = false;
            for (usize i = 0; i < schema.fields.size(); ++i)
            {
                if (schema.fields[i].name == it->first)
                {
                    known = true;
                    break;
                }
            }

            if (!known)
            {
                return {FieldMapperCode::unknown_extra_field, "Unknown extra field: " + it->first};
            }
        }
    }

    return {FieldMapperCode::ok, "Mapping successful."};
}

FieldMapperResult FieldMapper::map_ordered_to_named(
    const SchemaDef& schema,
    const OrderedPayload& ordered_payload,
    NamedPayload& out_named_payload
)
{
    out_named_payload.clear();

    if (ordered_payload.size() != schema.fields.size())
    {
        return {FieldMapperCode::ordered_value_count_mismatch, "Ordered payload field count does not match schema."};
    }

    for (usize i = 0; i < schema.fields.size(); ++i)
    {
        const FieldDef& field = schema.fields[i];
        const FieldValue& value = ordered_payload[i];

        if (!field_value_matches_type(value, field.type))
        {
            std::ostringstream oss;
            oss << "Field type mismatch for '" << field.name << "'. Expected "
                << field_type_to_string(field.type) << ", got "
                << field_type_to_string(value.type) << ".";
            return {FieldMapperCode::field_type_mismatch, oss.str()};
        }

        out_named_payload[field.name] = value;
    }

    return {FieldMapperCode::ok, "Reverse mapping successful."};
}

FieldMapperResult FieldMapper::encode_named_payload(
    const SchemaDef& schema,
    const NamedPayload& named_payload,
    Encoder& encoder
)
{
    OrderedPayload ordered_payload;
    const FieldMapperResult map_result = map_named_to_ordered(schema, named_payload, ordered_payload);
    if (!map_result.ok())
    {
        return map_result;
    }

    for (usize i = 0; i < ordered_payload.size(); ++i)
    {
        write_value(encoder, ordered_payload[i]);
    }

    return {FieldMapperCode::ok, "Encoding successful."};
}

}
