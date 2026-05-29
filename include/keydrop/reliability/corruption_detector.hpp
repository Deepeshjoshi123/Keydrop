#pragma once

#include <functional>
#include <string>
#include <vector>

#include "keydrop/core/buffer.hpp"
#include "keydrop/core/types.hpp"

namespace keydrop {

enum class CorruptionErrorCode {
    ok,
    minimum_length_violation,
    truncated_read,
    invalid_field_length,
    bad_header_marker,
    checksum_mismatch
};

struct CorruptionCheckResult {
    bool ok = true;
    CorruptionErrorCode error_code = CorruptionErrorCode::ok;
    std::string error_message;
    usize offset = 0;
};

struct CorruptionCheckOptions {
    usize minimum_length = 0;

    usize header_offset = 0;
    std::vector<byte> expected_header;

    bool enable_length_prefix = false;
    usize length_field_offset = 0;
    usize length_field_size = 0; // Supported: 1, 2, 4 bytes.
    usize payload_offset = 0;
    usize max_payload_length = 0; // 0 means no upper bound.
    bool strict_length_match = false;

    std::function<bool(const Buffer&)> checksum_validator;
};

class CorruptionDetector {
public:
    static CorruptionCheckResult check_packet(
        const Buffer& packet,
        const CorruptionCheckOptions& options
    );
};

}
