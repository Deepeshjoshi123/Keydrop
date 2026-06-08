#include "keydrop/reliability/packet_synchronizer.hpp"

#include <deque>

#include "keydrop/reliability/corruption_detector.hpp"
#include "keydrop/schema/runtime_optimizer.hpp"

namespace keydrop {

namespace {

constexpr byte kOptimizedMarker = 0xFD;
constexpr u16 kDictionaryStringReferenceMarker = 0xFFFF;

bool is_bit_set(const std::vector<byte>& data, usize bitmap_offset, usize index)
{
    const usize byte_index = bitmap_offset + (index / 8);
    const usize bit_index = index % 8;
    return (data[byte_index] & static_cast<byte>(1u << bit_index)) != 0;
}

u16 read_u16_le(const std::vector<byte>& data, usize offset)
{
    return static_cast<u16>(data[offset])
        | static_cast<u16>(static_cast<u16>(data[offset + 1]) << 8);
}

bool measure_variable_field(
    const FieldDef& field,
    const std::vector<byte>& data,
    usize& cursor
)
{
    if (cursor + 2 > data.size())
    {
        return false;
    }

    const u16 declared_size = read_u16_le(data, cursor);
    cursor += 2;

    if (
        field.type == FieldType::string
        &&
        declared_size == kDictionaryStringReferenceMarker
    )
    {
        if (cursor + 2 > data.size())
        {
            return false;
        }
        cursor += 2;
        return true;
    }

    if (
        field.constraints.has_max_length
        &&
        declared_size > field.constraints.max_length
    )
    {
        return false;
    }

    if (cursor + declared_size > data.size())
    {
        return false;
    }

    cursor += declared_size;
    return true;
}

bool measure_unoptimized_packet(
    const SchemaDef& schema,
    const std::vector<byte>& data,
    usize offset,
    usize& out_length
)
{
    if (offset + 2 > data.size())
    {
        return false;
    }

    usize cursor = offset + 2;
    for (usize i = 0; i < schema.fields.size(); ++i)
    {
        const FieldDef& field = schema.fields[i];

        usize fixed_size = 0;
        if (try_field_type_fixed_size(field.type, fixed_size))
        {
            if (cursor + fixed_size > data.size())
            {
                return false;
            }
            cursor += fixed_size;
            continue;
        }

        if (!measure_variable_field(field, data, cursor))
        {
            return false;
        }
    }

    out_length = cursor - offset;
    return true;
}

bool measure_optimized_packet(
    const SchemaDef& schema,
    const std::vector<byte>& data,
    usize offset,
    usize& out_length
)
{
    if (offset + 4 > data.size() || data[offset + 2] != kOptimizedMarker)
    {
        return false;
    }

    const usize bitmap_size = data[offset + 3];
    const usize expected_bitmap_size = (schema.fields.size() + 7) / 8;
    if (bitmap_size != expected_bitmap_size)
    {
        return false;
    }

    const usize bitmap_offset = offset + 4;
    if (bitmap_offset + bitmap_size > data.size())
    {
        return false;
    }

    usize cursor = bitmap_offset + bitmap_size;
    for (usize i = 0; i < schema.fields.size(); ++i)
    {
        const FieldDef& field = schema.fields[i];

        usize fixed_size = 0;
        if (try_field_type_fixed_size(field.type, fixed_size))
        {
            if (is_bit_set(data, bitmap_offset, i))
            {
                continue;
            }

            if (cursor + fixed_size > data.size())
            {
                return false;
            }
            cursor += fixed_size;
            continue;
        }

        if (!measure_variable_field(field, data, cursor))
        {
            return false;
        }
    }

    out_length = cursor - offset;
    return true;
}

Buffer slice_buffer(const std::vector<byte>& data, usize offset, usize length)
{
    Buffer out;
    if (length > 0)
    {
        out.append(&data[offset], length);
    }
    return out;
}

bool candidate_is_valid(
    const SchemaDef& schema,
    const Buffer& candidate
)
{
    Buffer validation_packet;
    const RuntimeOptimizerResult deoptimize_result =
        RuntimeOptimizer::deoptimize_packet(schema, candidate, validation_packet);
    if (!deoptimize_result.ok)
    {
        return false;
    }

    const CorruptionCheckResult corruption_check =
        CorruptionDetector::check_keydrop_packet(validation_packet, schema);
    return corruption_check.ok;
}

} // namespace

PacketSyncResult PacketSynchronizer::recover_next_packet(
    const Buffer& stream,
    const SchemaRegistry& registry
)
{
    const std::vector<byte>& data = stream.data();
    if (data.size() < 2)
    {
        PacketSyncResult result;
        result.code = PacketSyncStatusCode::no_packet_found;
        result.message = "No complete message_id candidate found.";
        return result;
    }

    for (usize offset = 0; offset + 2 <= data.size(); ++offset)
    {
        const u16 message_id = read_u16_le(data, offset);
        const SchemaDef* schema = registry.find_by_message_id(message_id);
        if (schema == nullptr)
        {
            continue;
        }

        usize packet_length = 0;
        bool measured = false;
        if (offset + 3 <= data.size() && data[offset + 2] == kOptimizedMarker)
        {
            measured = measure_optimized_packet(*schema, data, offset, packet_length);
        }
        else
        {
            measured = measure_unoptimized_packet(*schema, data, offset, packet_length);
        }

        if (!measured)
        {
            continue;
        }

        Buffer candidate = slice_buffer(data, offset, packet_length);
        if (!candidate_is_valid(*schema, candidate))
        {
            continue;
        }

        PacketSyncResult result;
        result.code = PacketSyncStatusCode::ok;
        result.message = "Recovered synchronized packet.";
        result.offset = offset;
        result.skipped_bytes = offset;
        result.message_id = message_id;
        result.schema_name = schema->schema_name;
        result.packet = candidate;
        return result;
    }

    PacketSyncResult result;
    result.code = PacketSyncStatusCode::no_packet_found;
    result.message = "No valid packet boundary found.";
    return result;
}

bool PacketSynchronizer::recover_all_packets(
    const Buffer& stream,
    const SchemaRegistry& registry,
    std::vector<PacketSyncResult>& out_packets
)
{
    out_packets.clear();

    Buffer remaining = stream;
    usize consumed = 0;
    while (!remaining.empty())
    {
        PacketSyncResult recovered = recover_next_packet(remaining, registry);
        if (!recovered.ok())
        {
            break;
        }

        recovered.offset += consumed;
        consumed += recovered.skipped_bytes + recovered.packet.size();
        out_packets.push_back(recovered);

        const std::vector<byte>& data = remaining.data();
        if (recovered.skipped_bytes + recovered.packet.size() >= data.size())
        {
            break;
        }

        Buffer next_remaining;
        const usize next_offset = recovered.skipped_bytes + recovered.packet.size();
        next_remaining.append(&data[next_offset], data.size() - next_offset);
        remaining = next_remaining;
    }

    return !out_packets.empty();
}

}
