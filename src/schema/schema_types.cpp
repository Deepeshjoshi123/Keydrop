#include "keydrop/schema/schema_types.hpp"

#include <unordered_map>

namespace keydrop {

bool try_parse_field_type(const std::string& value, FieldType& out_type)
{
    static const std::unordered_map<std::string, FieldType> mapping = {
        {"u8", FieldType::u8},
        {"u16", FieldType::u16},
        {"u32", FieldType::u32},
        {"i8", FieldType::i8},
        {"i16", FieldType::i16},
        {"i32", FieldType::i32},
        {"f32", FieldType::f32},
        {"f64", FieldType::f64},
        {"string", FieldType::string},
        {"bytes", FieldType::bytes},
    };

    const auto it = mapping.find(value);
    if (it == mapping.end())
    {
        return false;
    }

    out_type = it->second;
    return true;
}

const char* field_type_to_string(FieldType type)
{
    switch (type)
    {
    case FieldType::u8: return "u8";
    case FieldType::u16: return "u16";
    case FieldType::u32: return "u32";
    case FieldType::i8: return "i8";
    case FieldType::i16: return "i16";
    case FieldType::i32: return "i32";
    case FieldType::f32: return "f32";
    case FieldType::f64: return "f64";
    case FieldType::string: return "string";
    case FieldType::bytes: return "bytes";
    }

    return "unknown";
}

bool is_variable_length(FieldType type)
{
    return type == FieldType::string || type == FieldType::bytes;
}

bool try_field_type_fixed_size(FieldType type, usize& out_size)
{
    switch (type)
    {
    case FieldType::u8:
    case FieldType::i8:
        out_size = 1;
        return true;

    case FieldType::u16:
    case FieldType::i16:
        out_size = 2;
        return true;

    case FieldType::u32:
    case FieldType::i32:
    case FieldType::f32:
        out_size = 4;
        return true;

    case FieldType::f64:
        out_size = 8;
        return true;

    case FieldType::string:
    case FieldType::bytes:
        return false;
    }

    return false;
}

}
