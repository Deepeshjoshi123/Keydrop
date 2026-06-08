#include "keydrop/reliability/corruption_detector.hpp"

namespace keydrop {

namespace {

constexpr u16 kDictionaryStringReferenceMarker = 0xFFFF;

CorruptionCheckResult fail(
    CorruptionErrorCode code,
    const char* message,
    usize offset
)
{
    CorruptionCheckResult result;
    result.ok = false;
    result.error_code = code;
    result.error_message = message;
    result.offset = offset;
    return result;
}

u32 read_little_endian_value(
    const Buffer& packet,
    usize offset,
    usize size
)
{
    u32 value = 0;
    for (usize i = 0; i < size; ++i)
    {
        value |= static_cast<u32>(packet.read(offset + i)) << (8 * i);
    }
    return value;
}

u32 crc32_range(
    const std::vector<byte>& data,
    usize begin,
    usize end
)
{
    u32 crc = 0xFFFFFFFFu;
    for (usize i = begin; i < end; ++i)
    {
        crc ^= static_cast<u32>(data[i]);
        for (usize bit = 0; bit < 8; ++bit)
        {
            const u32 mask = static_cast<u32>(0u - (crc & 1u));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }

    return ~crc;
}

} // namespace

CorruptionCheckResult CorruptionDetector::check_packet(
    const Buffer& packet,
    const CorruptionCheckOptions& options
)
{
    if (packet.size() < options.minimum_length)
    {
        return fail(
            CorruptionErrorCode::minimum_length_violation,
            "Packet is smaller than minimum length",
            packet.size()
        );
    }

    if (!options.expected_header.empty())
    {
        const usize header_end =
            options.header_offset + options.expected_header.size();
        if (packet.size() < header_end)
        {
            return fail(
                CorruptionErrorCode::truncated_read,
                "Packet truncated before complete header marker",
                packet.size()
            );
        }

        for (usize i = 0; i < options.expected_header.size(); ++i)
        {
            if (packet.read(options.header_offset + i) != options.expected_header[i])
            {
                return fail(
                    CorruptionErrorCode::bad_header_marker,
                    "Header marker mismatch",
                    options.header_offset + i
                );
            }
        }
    }

    if (options.enable_length_prefix)
    {
        if (
            options.length_field_size != 1
            &&
            options.length_field_size != 2
            &&
            options.length_field_size != 4
        )
        {
            return fail(
                CorruptionErrorCode::invalid_field_length,
                "Unsupported length field size",
                options.length_field_offset
            );
        }

        const usize length_end =
            options.length_field_offset + options.length_field_size;
        if (packet.size() < length_end)
        {
            return fail(
                CorruptionErrorCode::truncated_read,
                "Packet truncated before length field",
                packet.size()
            );
        }

        const u32 declared_payload =
            read_little_endian_value(
                packet,
                options.length_field_offset,
                options.length_field_size
            );

        if (
            options.max_payload_length != 0
            &&
            declared_payload > options.max_payload_length
        )
        {
            return fail(
                CorruptionErrorCode::invalid_field_length,
                "Declared payload length exceeds maximum",
                options.length_field_offset
            );
        }

        const usize expected_total =
            options.payload_offset + declared_payload;
        if (packet.size() < expected_total)
        {
            return fail(
                CorruptionErrorCode::truncated_read,
                "Packet truncated relative to declared payload length",
                packet.size()
            );
        }

        if (
            options.strict_length_match
            &&
            packet.size() != expected_total
        )
        {
            return fail(
                CorruptionErrorCode::invalid_field_length,
                "Packet length does not match declared payload length",
                expected_total
            );
        }
    }

    if (
        options.checksum_validator
        &&
        !options.checksum_validator(packet)
    )
    {
        return fail(
            CorruptionErrorCode::checksum_mismatch,
            "Checksum validation failed",
            packet.size()
        );
    }

    if (options.enable_crc32)
    {
        if (options.crc32_offset + 4 > packet.size())
        {
            return fail(
                CorruptionErrorCode::truncated_read,
                "Packet truncated before CRC32 field",
                packet.size()
            );
        }

        const u32 expected_crc =
            read_little_endian_value(packet, options.crc32_offset, 4);
        const u32 actual_crc =
            crc32_range(packet.data(), 0, options.crc32_offset);

        if (expected_crc != actual_crc)
        {
            return fail(
                CorruptionErrorCode::checksum_mismatch,
                "CRC32 validation failed",
                options.crc32_offset
            );
        }

        if (
            options.strict_length_match
            &&
            packet.size() != options.crc32_offset + 4
        )
        {
            return fail(
                CorruptionErrorCode::invalid_field_length,
                "Packet has trailing bytes after CRC32 field",
                options.crc32_offset + 4
            );
        }
    }

    return CorruptionCheckResult{};
}

CorruptionCheckResult CorruptionDetector::check_keydrop_packet(
    const Buffer& packet,
    const SchemaDef& schema
)
{
    if (packet.size() < 2)
    {
        return fail(
            CorruptionErrorCode::minimum_length_violation,
            "Packet too small to contain Keydrop message_id",
            packet.size()
        );
    }

    const u16 message_id =
        static_cast<u16>(packet.data()[0])
        |
        (static_cast<u16>(packet.data()[1]) << 8);
    if (message_id != schema.message_id)
    {
        return fail(
            CorruptionErrorCode::message_id_mismatch,
            "Packet message_id does not match schema",
            0
        );
    }

    usize cursor = 2;
    for (usize i = 0; i < schema.fields.size(); ++i)
    {
        const FieldDef& field = schema.fields[i];

        usize fixed_size = 0;
        if (try_field_type_fixed_size(field.type, fixed_size))
        {
            if (cursor + fixed_size > packet.size())
            {
                return fail(
                    CorruptionErrorCode::truncated_read,
                    "Packet truncated inside fixed-size field",
                    cursor
                );
            }

            cursor += fixed_size;
            continue;
        }

        if (cursor + 2 > packet.size())
        {
            return fail(
                CorruptionErrorCode::truncated_read,
                "Packet truncated before variable field length",
                cursor
            );
        }

        const u16 declared_size =
            static_cast<u16>(packet.data()[cursor])
            |
            (static_cast<u16>(packet.data()[cursor + 1]) << 8);
        cursor += 2;

        if (
            field.type == FieldType::string
            &&
            declared_size == kDictionaryStringReferenceMarker
        )
        {
            if (cursor + 2 > packet.size())
            {
                return fail(
                    CorruptionErrorCode::truncated_read,
                    "Packet truncated inside dictionary string reference",
                    cursor
                );
            }

            cursor += 2;
            continue;
        }

        if (
            field.constraints.has_max_length
            &&
            declared_size > field.constraints.max_length
        )
        {
            return fail(
                CorruptionErrorCode::invalid_field_length,
                "Variable field length exceeds schema constraint",
                cursor - 2
            );
        }

        if (cursor + declared_size > packet.size())
        {
            return fail(
                CorruptionErrorCode::truncated_read,
                "Packet truncated inside variable field payload",
                cursor
            );
        }

        cursor += declared_size;
    }

    if (cursor != packet.size())
    {
        return fail(
            CorruptionErrorCode::packet_boundary_mismatch,
            "Packet has trailing bytes after schema fields",
            cursor
        );
    }

    return CorruptionCheckResult{};
}

u32 CorruptionDetector::crc32(const Buffer& packet)
{
    return crc32_range(packet.data(), 0, packet.size());
}

}
