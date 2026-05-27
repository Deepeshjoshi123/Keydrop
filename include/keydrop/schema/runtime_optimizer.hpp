#pragma once

#include "keydrop/core/buffer.hpp"
#include "keydrop/schema/schema_types.hpp"

namespace keydrop {

struct RuntimeOptimizerConfig {
    bool enabled = false;
    bool enable_zero_value_omission = true;
};

struct RuntimeOptimizerResult {
    bool ok = false;
    bool applied = false;
    usize bytes_saved = 0;
};

class RuntimeOptimizer {
public:
    static RuntimeOptimizerResult optimize_packet(
        const SchemaDef& schema,
        const Buffer& input_packet,
        Buffer& output_packet,
        const RuntimeOptimizerConfig& config
    );

    static RuntimeOptimizerResult deoptimize_packet(
        const SchemaDef& schema,
        const Buffer& input_packet,
        Buffer& output_packet
    );
    
    static bool is_optimized_packet(const Buffer& packet);

private:
    static constexpr byte kOptimizedMarker = 0xFD;
};

}
