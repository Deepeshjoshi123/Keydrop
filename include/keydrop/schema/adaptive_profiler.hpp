#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "keydrop/core/types.hpp"
#include "keydrop/schema/field_mapper.hpp"

namespace keydrop {

class SchemaRuntime;

struct AdaptiveProfilerConfig {
    bool enabled = false;
    usize window_size = 100;
    // Phase 4 decision rules: enable dictionary when repeated-string ratio
    // >= threshold and predictive dictionary hit rate >= threshold; enable
    // change-only/delta packets when the unchanged-field ratio >= threshold.
    f32 repeated_string_ratio_threshold = 0.6f;
    f32 dictionary_hit_rate_threshold = 0.8f;
    f32 unchanged_field_ratio_threshold = 0.4f;
};

// Windowed stream observer (Phase 4). Called on every successful send, it
// accumulates per-schema statistics and, at each window boundary, applies
// the predefined decision rules to runtime components the user has not
// explicitly configured. Explicit user configuration always wins.
class AdaptiveProfiler {
public:
    void configure(const AdaptiveProfilerConfig& config);
    const AdaptiveProfilerConfig& config() const;
    void reset();

    void observe(const std::string& schema_name, const NamedPayload& payload);
    void maybe_apply(const SchemaRuntime& runtime);

    // Current adaptive decisions (for tests and inspection).
    bool dictionary_decision() const { return dictionary_enabled_; }
    bool delta_decision() const { return delta_enabled_; }

private:
    struct WindowStats {
        usize samples = 0;
        usize string_observations = 0;
        usize repeated_strings = 0;
        usize predictive_hits = 0;
        usize unchanged_fields = 0;
        usize total_fields = 0;
        std::unordered_set<std::string> strings;
        NamedPayload last_payload;
    };

    void apply_decisions(const SchemaRuntime& runtime);

    AdaptiveProfilerConfig config_;
    std::unordered_map<std::string, WindowStats> stats_;
    std::unordered_map<std::string, std::unordered_set<std::string>> previous_strings_;
    bool dictionary_enabled_ = false;
    bool delta_enabled_ = false;
    bool decisions_applied_ = false;
};

}
