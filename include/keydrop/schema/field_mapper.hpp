#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "keydrop/core/encoder.hpp"
#include "keydrop/schema/schema_types.hpp"

namespace keydrop {

struct FieldValue {
    FieldType type = FieldType::u8;

    u8 as_u8 = 0;
    u16 as_u16 = 0;
    u32 as_u32 = 0;
    i8 as_i8 = 0;
    i16 as_i16 = 0;
    i32 as_i32 = 0;
    f32 as_f32 = 0.0f;
    f64 as_f64 = 0.0;
    std::string as_string;
    std::vector<byte> as_bytes;

    static FieldValue from_u8(u8 value);
    static FieldValue from_u16(u16 value);
    static FieldValue from_u32(u32 value);
    static FieldValue from_i8(i8 value);
    static FieldValue from_i16(i16 value);
    static FieldValue from_i32(i32 value);
    static FieldValue from_f32(f32 value);
    static FieldValue from_f64(f64 value);
    static FieldValue from_string(const std::string& value);
    static FieldValue from_bytes(const std::vector<byte>& value);
};

enum class FieldMapperCode {
    ok,
    missing_required_field,
    unknown_extra_field,
    field_type_mismatch,
    ordered_value_count_mismatch
};

struct FieldMapperResult {
    FieldMapperCode code = FieldMapperCode::ok;
    std::string message;

    bool ok() const
    {
        return code == FieldMapperCode::ok;
    }
};

using NamedPayload = std::unordered_map<std::string, FieldValue>;
using OrderedPayload = std::vector<FieldValue>;

class FieldMapper {
public:
    static FieldMapperResult map_named_to_ordered(
        const SchemaDef& schema,
        const NamedPayload& named_payload,
        OrderedPayload& out_ordered_payload
    );

    static FieldMapperResult map_ordered_to_named(
        const SchemaDef& schema,
        const OrderedPayload& ordered_payload,
        NamedPayload& out_named_payload
    );

    static FieldMapperResult encode_named_payload(
        const SchemaDef& schema,
        const NamedPayload& named_payload,
        Encoder& encoder
    );
};

}
