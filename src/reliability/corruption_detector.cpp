#include "keydrop/reliability/corruption_detector.hpp"

namespace keydrop {

namespace {

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

    return CorruptionCheckResult{};
}

}
