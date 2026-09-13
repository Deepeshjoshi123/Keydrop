#pragma once

#include <string>
#include <vector>

#include "keydrop/schema/field_mapper.hpp"
#include "keydrop/schema/schema_registry.hpp"
#include "keydrop/schema/schema_types.hpp"

namespace keydrop {

enum class SchemaValidationCode {
    ok,
    empty_schema_name,
    empty_fields,
    duplicate_field_name,
    non_contiguous_field_order,
    unsupported_field_type,
    message_id_out_of_range,
    duplicate_schema_name,
    duplicate_message_id,
    message_id_mismatch,
    payload_field_count_mismatch,
    payload_field_type_mismatch,
    payload_field_constraint_violation
};

struct SchemaValidationResult {
    SchemaValidationCode code = SchemaValidationCode::ok;
    std::string message;

    bool ok() const
    {
        return code == SchemaValidationCode::ok;
    }
};

class SchemaValidator {
public:
    static SchemaValidationResult validate_schema(
        const SchemaDef& schema,
        const SchemaRegistry* registry = nullptr
    );

    static SchemaValidationResult validate_payload_field_types(
        const SchemaDef& schema,
        const std::vector<FieldType>& payload_field_types
    );

    static SchemaValidationResult validate_message_id(
        const SchemaDef& schema,
        u16 message_id
    );

    static SchemaValidationResult validate_payload_values(
        const SchemaDef& schema,
        const OrderedPayload& payload
    );

private:
    static bool is_supported_field_type(FieldType type);
};

}
