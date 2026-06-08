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

static void write_u16(Buffer& packet, u16 value)
{
    packet.write(static_cast<byte>(value & 0xFF));
    packet.write(static_cast<byte>((value >> 8) & 0xFF));
}

static void append_crc32(Buffer& packet)
{
    const u32 crc = CorruptionDetector::crc32(packet);
    packet.write(static_cast<byte>(crc & 0xFF));
    packet.write(static_cast<byte>((crc >> 8) & 0xFF));
    packet.write(static_cast<byte>((crc >> 16) & 0xFF));
    packet.write(static_cast<byte>((crc >> 24) & 0xFF));
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

    {
        Buffer packet = make_packet({0xAA, 0x02, 0x00, 0x10, 0x20});
        append_crc32(packet);

        CorruptionCheckOptions options;
        options.enable_crc32 = true;
        options.crc32_offset = 5;
        options.strict_length_match = true;

        const CorruptionCheckResult result =
            CorruptionDetector::check_packet(packet, options);
        assert(result.ok);
    }

    {
        const Buffer packet = make_packet({0xAA, 0x02, 0x00, 0x10, 0x20, 0, 0, 0, 0});
        CorruptionCheckOptions options;
        options.enable_crc32 = true;
        options.crc32_offset = 5;
        options.strict_length_match = true;

        const CorruptionCheckResult result =
            CorruptionDetector::check_packet(packet, options);
        assert(!result.ok);
        assert(result.error_code == CorruptionErrorCode::checksum_mismatch);
    }

    const SchemaDef boundary_schema {
        "BoundaryData",
        42,
        {
            FieldDef {"temperature", FieldType::u8, 0, {}},
            FieldDef {"label", FieldType::string, 1, FieldConstraints {true, 8}},
            FieldDef {"raw", FieldType::bytes, 2, FieldConstraints {true, 4}},
        }
    };

    {
        Buffer packet;
        write_u16(packet, 42);
        packet.write(32);
        write_u16(packet, 3);
        packet.write('a');
        packet.write('b');
        packet.write('c');
        write_u16(packet, 2);
        packet.write(0x10);
        packet.write(0x20);

        const CorruptionCheckResult result =
            CorruptionDetector::check_keydrop_packet(packet, boundary_schema);
        assert(result.ok);
    }

    {
        Buffer packet;
        write_u16(packet, 42);
        packet.write(32);
        write_u16(packet, 12);
        packet.write('a');

        const CorruptionCheckResult result =
            CorruptionDetector::check_keydrop_packet(packet, boundary_schema);
        assert(!result.ok);
        assert(result.error_code == CorruptionErrorCode::invalid_field_length);
    }

    {
        Buffer packet;
        write_u16(packet, 42);
        packet.write(32);
        write_u16(packet, 3);
        packet.write('a');
        packet.write('b');

        const CorruptionCheckResult result =
            CorruptionDetector::check_keydrop_packet(packet, boundary_schema);
        assert(!result.ok);
        assert(result.error_code == CorruptionErrorCode::truncated_read);
    }

    {
        Buffer packet;
        write_u16(packet, 42);
        packet.write(32);
        write_u16(packet, 3);
        packet.write('a');
        packet.write('b');
        packet.write('c');
        write_u16(packet, 4);
        packet.write(0x10);

        const CorruptionCheckResult result =
            CorruptionDetector::check_keydrop_packet(packet, boundary_schema);
        assert(!result.ok);
        assert(result.error_code == CorruptionErrorCode::truncated_read);
    }

    {
        Buffer packet;
        write_u16(packet, 99);
        packet.write(32);
        write_u16(packet, 0);
        write_u16(packet, 0);

        const CorruptionCheckResult result =
            CorruptionDetector::check_keydrop_packet(packet, boundary_schema);
        assert(!result.ok);
        assert(result.error_code == CorruptionErrorCode::message_id_mismatch);
    }

    return 0;
}
