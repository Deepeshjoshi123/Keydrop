#include "keydrop/schema/json_types.hpp"

namespace keydrop {

JsonValue JsonValue::from_integer(i64 value)
{
    JsonValue out;
    out.type = JsonValueType::integer;
    out.integer_value = value;
    return out;
}

JsonValue JsonValue::from_decimal(f64 value)
{
    JsonValue out;
    out.type = JsonValueType::decimal;
    out.decimal_value = value;
    return out;
}

JsonValue JsonValue::from_string(const std::string& value)
{
    JsonValue out;
    out.type = JsonValueType::string;
    out.string_value = value;
    return out;
}

JsonValue JsonValue::from_bytes(const std::vector<byte>& value)
{
    JsonValue out;
    out.type = JsonValueType::bytes;
    out.bytes_value = value;
    return out;
}

}
