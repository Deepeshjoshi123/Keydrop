#pragma once

#include <string>
#include <vector>

#include "keydrop/core/types.hpp"

namespace keydrop {

enum class FieldType {
    u8,
    u16,
    u32,
    i8,
    i16,
    i32,
    f32,
    f64,
    string,
    bytes
};

struct FieldConstraints {
    bool has_max_length = false;
    usize max_length = 0;
};

struct FieldDef {
    std::string name;
    FieldType type;
    usize index;
    FieldConstraints constraints;
};

struct SchemaDef {
    std::string schema_name;
    u16 message_id;
    std::vector<FieldDef> fields;
};

bool try_parse_field_type(const std::string& value, FieldType& out_type);
const char* field_type_to_string(FieldType type);

bool is_variable_length(FieldType type);
bool try_field_type_fixed_size(FieldType type, usize& out_size);

}
