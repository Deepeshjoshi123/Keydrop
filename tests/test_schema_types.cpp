#include <cassert>

#include "keydrop/schema/schema_types.hpp"

using namespace keydrop;

int main()
{
    // Parsing checks
    FieldType parsed_type = FieldType::u8;
    const bool parsed_u8 = try_parse_field_type("u8", parsed_type);
    assert(parsed_u8);
    assert(parsed_type == FieldType::u8);

    const bool parsed_string = try_parse_field_type("string", parsed_type);
    assert(parsed_string);
    assert(parsed_type == FieldType::string);

    const bool parsed_unknown = try_parse_field_type("u128", parsed_type);
    assert(!parsed_unknown);

    // To-string checks
    assert(std::string(field_type_to_string(FieldType::f32)) == "f32");
    assert(std::string(field_type_to_string(FieldType::bytes)) == "bytes");

    // Fixed-size checks
    usize fixed_size = 0;
    const bool has_u16_size = try_field_type_fixed_size(FieldType::u16, fixed_size);
    assert(has_u16_size);
    assert(fixed_size == 2);

    const bool has_f64_size = try_field_type_fixed_size(FieldType::f64, fixed_size);
    assert(has_f64_size);
    assert(fixed_size == 8);

    // Variable length checks
    assert(is_variable_length(FieldType::string));
    assert(is_variable_length(FieldType::bytes));
    assert(!is_variable_length(FieldType::u32));

    const bool has_string_size = try_field_type_fixed_size(FieldType::string, fixed_size);
    assert(!has_string_size);

    // Model shape checks
    SchemaDef schema {
        "SensorData",
        1,
        {
            FieldDef {"temperature", FieldType::u8, 0, {}},
            FieldDef {"humidity", FieldType::u8, 1, {}},
            FieldDef {"device_id", FieldType::string, 2, FieldConstraints {true, 32}},
        }
    };

    assert(schema.schema_name == "SensorData");
    assert(schema.message_id == 1);
    assert(schema.fields.size() == 3);
    assert(schema.fields[2].constraints.has_max_length);
    assert(schema.fields[2].constraints.max_length == 32);

    return 0;
}
