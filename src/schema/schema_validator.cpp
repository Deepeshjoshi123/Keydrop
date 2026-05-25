#include "keydrop/schema/schema_validator.hpp"

#include <sstream>
#include <unordered_set>

namespace keydrop {

SchemaValidationResult SchemaValidator::validate_schema(
    const SchemaDef& schema,
    const SchemaRegistry* registry
)
{
    if (schema.schema_name.empty())
    {
        return {SchemaValidationCode::empty_schema_name, "Schema name cannot be empty."};
    }

    if (schema.fields.empty())
    {
        return {SchemaValidationCode::empty_fields, "Schema must contain at least one field."};
    }

    if (schema.message_id == 0)
    {
        return {SchemaValidationCode::message_id_out_of_range, "Schema message_id must be in range 1..65535."};
    }

    std::unordered_set<std::string> field_names;
    for (usize i = 0; i < schema.fields.size(); ++i)
    {
        const FieldDef& field = schema.fields[i];

        if (field.index != i)
        {
            return {SchemaValidationCode::non_contiguous_field_order, "Field indexes must be contiguous and start at 0."};
        }

        if (field_names.find(field.name) != field_names.end())
        {
            return {SchemaValidationCode::duplicate_field_name, "Duplicate field name found: " + field.name};
        }
        field_names.insert(field.name);

        if (!is_supported_field_type(field.type))
        {
            return {SchemaValidationCode::unsupported_field_type, "Unsupported field type for field: " + field.name};
        }
    }

    if (registry != nullptr)
    {
        const SchemaDef* same_name = registry->find_by_name(schema.schema_name);
        if (same_name != nullptr && same_name->message_id != schema.message_id)
        {
            return {SchemaValidationCode::duplicate_schema_name, "Schema name already exists in registry: " + schema.schema_name};
        }

        const SchemaDef* same_message_id = registry->find_by_message_id(schema.message_id);
        if (same_message_id != nullptr && same_message_id->schema_name != schema.schema_name)
        {
            return {SchemaValidationCode::duplicate_message_id, "Schema message_id already exists in registry."};
        }
    }

    return {SchemaValidationCode::ok, "Schema is valid."};
}

SchemaValidationResult SchemaValidator::validate_payload_field_types(
    const SchemaDef& schema,
    const std::vector<FieldType>& payload_field_types
)
{
    if (payload_field_types.size() != schema.fields.size())
    {
        std::ostringstream oss;
        oss << "Field count mismatch. Expected " << schema.fields.size()
            << ", got " << payload_field_types.size() << ".";
        return {SchemaValidationCode::payload_field_count_mismatch, oss.str()};
    }

    for (usize i = 0; i < schema.fields.size(); ++i)
    {
        const FieldType expected = schema.fields[i].type;
        const FieldType actual = payload_field_types[i];
        if (expected != actual)
        {
            std::ostringstream oss;
            oss << "Field type mismatch at index " << i
                << ". Expected " << field_type_to_string(expected)
                << ", got " << field_type_to_string(actual) << ".";
            return {SchemaValidationCode::payload_field_type_mismatch, oss.str()};
        }
    }

    return {SchemaValidationCode::ok, "Payload field types are compatible with schema."};
}

bool SchemaValidator::is_supported_field_type(FieldType type)
{
    switch (type)
    {
    case FieldType::u8:
    case FieldType::u16:
    case FieldType::u32:
    case FieldType::i8:
    case FieldType::i16:
    case FieldType::i32:
    case FieldType::f32:
    case FieldType::f64:
    case FieldType::string:
    case FieldType::bytes:
        return true;
    }

    return false;
}

}
