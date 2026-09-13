#include <cassert>
#include <vector>

#include "keydrop/schema/schema_registry.hpp"
#include "keydrop/schema/schema_validator.hpp"

using namespace keydrop;

int main()
{
    const SchemaDef valid_schema {
        "SensorData",
        1,
        {
            FieldDef {"temperature", FieldType::u8, 0, {}},
            FieldDef {"humidity", FieldType::u8, 1, {}},
            FieldDef {"device_id", FieldType::string, 2, FieldConstraints {true, 32}},
        }
    };

    const SchemaValidationResult valid_result = SchemaValidator::validate_schema(valid_schema, nullptr);
    assert(valid_result.ok());

    const SchemaDef empty_name {
        "",
        1,
        {
            FieldDef {"temperature", FieldType::u8, 0, {}},
        }
    };
    assert(SchemaValidator::validate_schema(empty_name, nullptr).code == SchemaValidationCode::empty_schema_name);

    const SchemaDef empty_fields {
        "NoFields",
        2,
        {}
    };
    assert(SchemaValidator::validate_schema(empty_fields, nullptr).code == SchemaValidationCode::empty_fields);

    const SchemaDef duplicate_fields {
        "DuplicateFields",
        3,
        {
            FieldDef {"value", FieldType::u16, 0, {}},
            FieldDef {"value", FieldType::u32, 1, {}},
        }
    };
    assert(SchemaValidator::validate_schema(duplicate_fields, nullptr).code == SchemaValidationCode::duplicate_field_name);

    const SchemaDef bad_order {
        "BadOrder",
        4,
        {
            FieldDef {"first", FieldType::u8, 0, {}},
            FieldDef {"second", FieldType::u8, 2, {}},
        }
    };
    assert(SchemaValidator::validate_schema(bad_order, nullptr).code == SchemaValidationCode::non_contiguous_field_order);

    const SchemaDef bad_message_id {
        "BadMessageId",
        0,
        {
            FieldDef {"value", FieldType::u8, 0, {}},
        }
    };
    assert(SchemaValidator::validate_schema(bad_message_id, nullptr).code == SchemaValidationCode::message_id_out_of_range);

    // Force unsupported value via cast to verify validator guard.
    const SchemaDef unsupported_type {
        "UnsupportedType",
        5,
        {
            FieldDef {"value", static_cast<FieldType>(999), 0, {}},
        }
    };
    assert(SchemaValidator::validate_schema(unsupported_type, nullptr).code == SchemaValidationCode::unsupported_field_type);

    SchemaRegistry registry;
    assert(registry.register_schema(valid_schema).ok());

    const SchemaDef same_name_new_id {
        "SensorData",
        20,
        {
            FieldDef {"temperature", FieldType::u8, 0, {}},
        }
    };
    assert(SchemaValidator::validate_schema(same_name_new_id, &registry).code == SchemaValidationCode::duplicate_schema_name);

    const SchemaDef new_name_same_id {
        "EnvData",
        1,
        {
            FieldDef {"pressure", FieldType::u32, 0, {}},
        }
    };
    assert(SchemaValidator::validate_schema(new_name_same_id, &registry).code == SchemaValidationCode::duplicate_message_id);

    // Payload compatibility checks for pre-decode bridge.
    const std::vector<FieldType> good_payload = {FieldType::u8, FieldType::u8, FieldType::string};
    assert(SchemaValidator::validate_payload_field_types(valid_schema, good_payload).ok());

    const std::vector<FieldType> bad_count_payload = {FieldType::u8, FieldType::u8};
    assert(SchemaValidator::validate_payload_field_types(valid_schema, bad_count_payload).code
           == SchemaValidationCode::payload_field_count_mismatch);

    const std::vector<FieldType> bad_type_payload = {FieldType::u8, FieldType::u16, FieldType::string};
    assert(SchemaValidator::validate_payload_field_types(valid_schema, bad_type_payload).code
           == SchemaValidationCode::payload_field_type_mismatch);

    assert(SchemaValidator::validate_message_id(valid_schema, 1).ok());
    assert(SchemaValidator::validate_message_id(valid_schema, 99).code
           == SchemaValidationCode::message_id_mismatch);

    OrderedPayload valid_values;
    valid_values.push_back(FieldValue::from_u8(32));
    valid_values.push_back(FieldValue::from_u8(70));
    valid_values.push_back(FieldValue::from_string("sensor_01"));
    assert(SchemaValidator::validate_payload_values(valid_schema, valid_values).ok());

    OrderedPayload short_values;
    short_values.push_back(FieldValue::from_u8(32));
    short_values.push_back(FieldValue::from_u8(70));
    assert(SchemaValidator::validate_payload_values(valid_schema, short_values).code
           == SchemaValidationCode::payload_field_count_mismatch);

    OrderedPayload wrong_type_values = valid_values;
    wrong_type_values[1] = FieldValue::from_u16(70);
    assert(SchemaValidator::validate_payload_values(valid_schema, wrong_type_values).code
           == SchemaValidationCode::payload_field_type_mismatch);

    OrderedPayload long_string_values = valid_values;
    long_string_values[2] = FieldValue::from_string("sensor_name_that_is_too_long_for_schema");
    assert(SchemaValidator::validate_payload_values(valid_schema, long_string_values).code
           == SchemaValidationCode::payload_field_constraint_violation);

    const SchemaDef bytes_schema {
        "BytesData",
        6,
        {
            FieldDef {"raw", FieldType::bytes, 0, FieldConstraints {true, 2}},
        }
    };
    OrderedPayload long_bytes_values;
    long_bytes_values.push_back(FieldValue::from_bytes({0x01, 0x02, 0x03}));
    assert(SchemaValidator::validate_payload_values(bytes_schema, long_bytes_values).code
           == SchemaValidationCode::payload_field_constraint_violation);

    return 0;
}
