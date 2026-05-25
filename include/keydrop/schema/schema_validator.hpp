#pragma once

#include <string>
#include <vector>

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
    payload_field_count_mismatch,
    payload_field_type_mismatch
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

private:
    static bool is_supported_field_type(FieldType type);
};

}
