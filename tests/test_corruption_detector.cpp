#include <cassert>

#include "keydrop/core/buffer.hpp"
#include "keydrop/reliability/corruption_detector.hpp"

using namespace keydrop;

static Buffer make_packet(std::initializer_list<byte> bytes)
{
    Buffer packet;
    for (byte b : bytes)
    {
        packet.write(b);
    }
    return packet;
}

int main()
{
    {
        const Buffer packet = make_packet({0xAA, 0x02, 0x00, 0x10, 0x20});
        CorruptionCheckOptions options;
        options.minimum_length = 5;
        options.expected_header = {0xAA};
        options.enable_length_prefix = true;
        options.length_field_offset = 1;
        options.length_field_size = 2;
        options.payload_offset = 3;
        options.max_payload_length = 64;
        options.strict_length_match = true;
        options.checksum_validator = [](const Buffer&) { return true; };

        const CorruptionCheckResult result =
            CorruptionDetector::check_packet(packet, options);
        assert(result.ok);
        assert(result.error_code == CorruptionErrorCode::ok);
    }

    {
        const Buffer packet = make_packet({0xAA});
        CorruptionCheckOptions options;
        options.minimum_length = 2;

        const CorruptionCheckResult result =
            CorruptionDetector::check_packet(packet, options);
        assert(!result.ok);
        assert(
            result.error_code
            ==
            CorruptionErrorCode::minimum_length_violation
        );
    }

    {
        const Buffer packet = make_packet({0xAB});
        CorruptionCheckOptions options;
        options.expected_header = {0xAA};

        const CorruptionCheckResult result =
            CorruptionDetector::check_packet(packet, options);
        assert(!result.ok);
        assert(
            result.error_code
            ==
            CorruptionErrorCode::bad_header_marker
        );
    }

    {
        const Buffer packet = make_packet({0xAA, 0x05, 0x00, 0x10, 0x20});
        CorruptionCheckOptions options;
        options.enable_length_prefix = true;
        options.length_field_offset = 1;
        options.length_field_size = 2;
        options.payload_offset = 3;

        const CorruptionCheckResult result =
            CorruptionDetector::check_packet(packet, options);
        assert(!result.ok);
        assert(result.error_code == CorruptionErrorCode::truncated_read);
    }

    {
        const Buffer packet = make_packet({0xAA, 0x05, 0x00, 0x10, 0x20});
        CorruptionCheckOptions options;
        options.enable_length_prefix = true;
        options.length_field_offset = 1;
        options.length_field_size = 2;
        options.payload_offset = 3;
        options.max_payload_length = 2;

        const CorruptionCheckResult result =
            CorruptionDetector::check_packet(packet, options);
        assert(!result.ok);
        assert(
            result.error_code
            ==
            CorruptionErrorCode::invalid_field_length
        );
    }

    {
        const Buffer packet = make_packet({0xAA, 0x02, 0x00, 0x10, 0x20});
        CorruptionCheckOptions options;
        options.checksum_validator = [](const Buffer&) { return false; };

        const CorruptionCheckResult result =
            CorruptionDetector::check_packet(packet, options);
        assert(!result.ok);
        assert(result.error_code == CorruptionErrorCode::checksum_mismatch);
    }

    return 0;
}
