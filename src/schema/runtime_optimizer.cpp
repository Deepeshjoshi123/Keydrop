#include "keydrop/schema/runtime_optimizer.hpp"

#include <vector>

#include "keydrop/schema/schema_types.hpp"

namespace keydrop {

namespace {

// String fields may carry a dictionary reference: 0xFFFF length marker
// followed by a 2-byte id. Like the corruption detector and packet
// synchronizer, the optimizer must treat it as a fixed 4-byte span, not a
// 65535-byte string payload.
constexpr u16 kDictionaryStringReferenceMarker = 0xFFFF;

bool is_bit_set(const std::vector<byte>& bitmap, usize index)
{
    const usize byte_index = index / 8;
    const usize bit_index = index % 8;
    return (bitmap[byte_index] & static_cast<byte>(1u << bit_index)) != 0;
}

void set_bit(std::vector<byte>& bitmap, usize index)
{
    const usize byte_index = index / 8;
    const usize bit_index = index % 8;
    bitmap[byte_index] = static_cast<byte>(bitmap[byte_index] | static_cast<byte>(1u << bit_index));
}

bool all_zero(const std::vector<byte>& data, usize offset, usize size)
{
    for (usize i = 0; i < size; ++i)
    {
        if (data[offset + i] != 0)
        {
            return false;
        }
    }
    return true;
}

} // namespace

RuntimeOptimizerResult RuntimeOptimizer::optimize_packet(
    const SchemaDef& schema,
    const Buffer& input_packet,
    Buffer& output_packet,
    const RuntimeOptimizerConfig& config
)
{
    if (!config.enabled || !config.enable_zero_value_omission)
    {
        return {true, false, 0};
    }

    if (input_packet.size() < 2 || schema.fields.size() > 2040)
    {
        return {false, false, 0};
    }

    output_packet = input_packet;

    const std::vector<byte>& bytes = input_packet.data();
    usize cursor = 2;
    std::vector<byte> bitmap((schema.fields.size() + 7) / 8, 0);
    std::vector<byte> optimized_body;

    for (usize i = 0; i < schema.fields.size(); ++i)
    {
        usize fixed_size = 0;
        if (try_field_type_fixed_size(schema.fields[i].type, fixed_size))
        {
            if (cursor + fixed_size > bytes.size())
            {
                return {false, false, 0};
            }

            if (all_zero(bytes, cursor, fixed_size))
            {
                set_bit(bitmap, i);
                cursor += fixed_size;
                continue;
            }

            for (usize j = 0; j < fixed_size; ++j)
            {
                optimized_body.push_back(bytes[cursor + j]);
            }
            cursor += fixed_size;
            continue;
        }

        if (schema.fields[i].type == FieldType::string || schema.fields[i].type == FieldType::bytes)
        {
            if (cursor + 2 > bytes.size())
            {
                return {false, false, 0};
            }

            const u16 data_size = static_cast<u16>(bytes[cursor]) | (static_cast<u16>(bytes[cursor + 1]) << 8);
            const bool dictionary_reference =
                schema.fields[i].type == FieldType::string && data_size == kDictionaryStringReferenceMarker;
            const usize total_size = dictionary_reference ? 4 : static_cast<usize>(2 + data_size);
            if (cursor + total_size > bytes.size())
            {
                return {false, false, 0};
            }

            for (usize j = 0; j < total_size; ++j)
            {
                optimized_body.push_back(bytes[cursor + j]);
            }
            cursor += total_size;
            continue;
        }

        return {false, false, 0};
    }

    if (cursor != bytes.size())
    {
        return {false, false, 0};
    }

    Buffer candidate;
    candidate.write(bytes[0]);
    candidate.write(bytes[1]);
    candidate.write(kOptimizedMarker);
    candidate.write(static_cast<byte>(bitmap.size()));
    candidate.append(bitmap.data(), bitmap.size());
    if (!optimized_body.empty())
    {
        candidate.append(optimized_body.data(), optimized_body.size());
    }

    if (candidate.size() >= input_packet.size())
    {
        return {true, false, 0};
    }

    output_packet = candidate;
    return {true, true, input_packet.size() - candidate.size()};
}

RuntimeOptimizerResult RuntimeOptimizer::deoptimize_packet(
    const SchemaDef& schema,
    const Buffer& input_packet,
    Buffer& output_packet
)
{
    if (!is_optimized_packet(input_packet))
    {
        output_packet = input_packet;
        return {true, false, 0};
    }

    const std::vector<byte>& bytes = input_packet.data();
    if (bytes.size() < 4)
    {
        return {false, false, 0};
    }

    const usize bitmap_size = bytes[3];
    const usize expected_bitmap_size = (schema.fields.size() + 7) / 8;
    if (bitmap_size != expected_bitmap_size || bytes.size() < (4 + bitmap_size))
    {
        return {false, false, 0};
    }

    std::vector<byte> bitmap(bitmap_size, 0);
    for (usize i = 0; i < bitmap_size; ++i)
    {
        bitmap[i] = bytes[4 + i];
    }

    usize cursor = 4 + bitmap_size;
    Buffer restored;
    restored.write(bytes[0]);
    restored.write(bytes[1]);

    for (usize i = 0; i < schema.fields.size(); ++i)
    {
        usize fixed_size = 0;
        if (try_field_type_fixed_size(schema.fields[i].type, fixed_size))
        {
            if (is_bit_set(bitmap, i))
            {
                for (usize j = 0; j < fixed_size; ++j)
                {
                    restored.write(0);
                }
            }
            else
            {
                if (cursor + fixed_size > bytes.size())
                {
                    return {false, false, 0};
                }
                for (usize j = 0; j < fixed_size; ++j)
                {
                    restored.write(bytes[cursor + j]);
                }
                cursor += fixed_size;
            }
            continue;
        }

        if (schema.fields[i].type == FieldType::string || schema.fields[i].type == FieldType::bytes)
        {
            if (cursor + 2 > bytes.size())
            {
                return {false, false, 0};
            }

            const u16 data_size = static_cast<u16>(bytes[cursor]) | (static_cast<u16>(bytes[cursor + 1]) << 8);
            const bool dictionary_reference =
                schema.fields[i].type == FieldType::string && data_size == kDictionaryStringReferenceMarker;
            const usize total_size = dictionary_reference ? 4 : static_cast<usize>(2 + data_size);
            if (cursor + total_size > bytes.size())
            {
                return {false, false, 0};
            }

            for (usize j = 0; j < total_size; ++j)
            {
                restored.write(bytes[cursor + j]);
            }
            cursor += total_size;
            continue;
        }

        return {false, false, 0};
    }

    if (cursor != bytes.size())
    {
        return {false, false, 0};
    }

    output_packet = restored;
    return {true, true, input_packet.size() - restored.size()};
}

bool RuntimeOptimizer::is_optimized_packet(const Buffer& packet)
{
    return packet.size() >= 3 && packet.data()[2] == kOptimizedMarker;
}

}
