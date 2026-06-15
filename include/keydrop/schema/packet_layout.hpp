#pragma once

#include <vector>

#include "keydrop/schema/schema_types.hpp"

namespace keydrop {

enum class FieldCodec : u8 {
    u8_value = 0,
    u16_value,
    u32_value,
    i8_value,
    i16_value,
    i32_value,
    f32_value,
    f64_value,
    string_value,
    bytes_value,
    count
};

struct FieldLayout {
    FieldType type = FieldType::u8;
    FieldCodec codec = FieldCodec::u8_value;
    usize schema_index = 0;
    usize fixed_size = 0;
    usize byte_offset = 0;
    bool variable_length = false;
    bool dynamic_offset = false;
    bool has_max_length = false;
    usize max_length = 0;
};

struct PacketLayout {
    u16 message_id = 0;
    std::vector<FieldLayout> fields;
    usize minimum_packet_size = 2;
    usize fixed_payload_bytes = 0;
    usize variable_field_count = 0;
    usize fixed_packet_size = 0;
    bool fixed_size_only = true;
};

PacketLayout build_packet_layout(const SchemaDef& schema);

}
