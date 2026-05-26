#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "keydrop/core/types.hpp"

namespace keydrop {

enum class JsonValueType {
    integer,
    decimal,
    string,
    bytes
};

struct JsonValue {
    JsonValueType type = JsonValueType::integer;
    i64 integer_value = 0;
    f64 decimal_value = 0.0;
    std::string string_value;
    std::vector<byte> bytes_value;

    static JsonValue from_integer(i64 value);
    static JsonValue from_decimal(f64 value);
    static JsonValue from_string(const std::string& value);
    static JsonValue from_bytes(const std::vector<byte>& value);
};

using JsonObject = std::unordered_map<std::string, JsonValue>;

}
