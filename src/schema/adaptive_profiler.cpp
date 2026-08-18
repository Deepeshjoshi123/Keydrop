#include "keydrop/schema/adaptive_profiler.hpp"

#include "keydrop/schema/schema_runtime.hpp"

namespace keydrop {

namespace {

bool field_values_equal(const FieldValue& left, const FieldValue& right)
{
    if (left.type != right.type)
    {
        return false;
    }

    switch (left.type)
    {
    case FieldType::u8: return left.as_u8 == right.as_u8;
    case FieldType::u16: return left.as_u16 == right.as_u16;
    case FieldType::u32: return left.as_u32 == right.as_u32;
    case FieldType::i8: return left.as_i8 == right.as_i8;
    case FieldType::i16: return left.as_i16 == right.as_i16;
    case FieldType::i32: return left.as_i32 == right.as_i32;
    case FieldType::f32: return left.as_f32 == right.as_f32;
    case FieldType::f64: return left.as_f64 == right.as_f64;
    case FieldType::string: return left.as_string == right.as_string;
    case FieldType::bytes: return left.as_bytes == right.as_bytes;
    }
    return false;
}

} // namespace

void AdaptiveProfiler::configure(const AdaptiveProfilerConfig& config)
{
    config_ = config;
    if (config_.window_size == 0)
    {
        config_.window_size = 1;
    }
}

const AdaptiveProfilerConfig& AdaptiveProfiler::config() const
{
    return config_;
}

void AdaptiveProfiler::reset()
{
    stats_.clear();
    previous_strings_.clear();
    dictionary_enabled_ = false;
    delta_enabled_ = false;
    decisions_applied_ = false;
}

void AdaptiveProfiler::observe(const std::string& schema_name, const NamedPayload& payload)
{
    if (!config_.enabled)
    {
        return;
    }

    WindowStats& stats = stats_[schema_name];
    const std::unordered_set<std::string>& previous = previous_strings_[schema_name];

    for (NamedPayload::const_iterator it = payload.begin(); it != payload.end(); ++it)
    {
        stats.total_fields += 1;

        const NamedPayload::const_iterator last_it = stats.last_payload.find(it->first);
        if (last_it != stats.last_payload.end() && field_values_equal(last_it->second, it->second))
        {
            stats.unchanged_fields += 1;
        }

        if (it->second.type != FieldType::string)
        {
            continue;
        }

        stats.string_observations += 1;
        if (stats.strings.find(it->second.as_string) != stats.strings.end())
        {
            stats.repeated_strings += 1;
        }
        if (previous.find(it->second.as_string) != previous.end())
        {
            stats.predictive_hits += 1;
        }
        stats.strings.insert(it->second.as_string);
    }

    stats.last_payload = payload;
    stats.samples += 1;
}

void AdaptiveProfiler::maybe_apply(const SchemaRuntime& runtime)
{
    if (!config_.enabled)
    {
        return;
    }

    bool changed = false;
    bool window_completed = false;
    for (std::unordered_map<std::string, WindowStats>::iterator it = stats_.begin(); it != stats_.end(); ++it)
    {
        WindowStats& stats = it->second;
        if (stats.samples < config_.window_size)
        {
            continue;
        }
        window_completed = true;

        const f32 repeated_ratio = stats.string_observations > 0
            ? static_cast<f32>(stats.repeated_strings) / static_cast<f32>(stats.string_observations)
            : 0.0f;
        // First window has no previous-window set to predict against, so
        // within-window repetition bootstraps the hit-rate estimate.
        const usize hit_observations = previous_strings_[it->first].empty()
            ? stats.repeated_strings
            : stats.predictive_hits;
        const f32 hit_rate = stats.string_observations > 0
            ? static_cast<f32>(hit_observations) / static_cast<f32>(stats.string_observations)
            : 0.0f;
        const f32 unchanged_ratio = stats.total_fields > 0
            ? static_cast<f32>(stats.unchanged_fields) / static_cast<f32>(stats.total_fields)
            : 0.0f;

        const bool want_dictionary =
            repeated_ratio >= config_.repeated_string_ratio_threshold
            && hit_rate >= config_.dictionary_hit_rate_threshold;
        const bool want_delta = unchanged_ratio >= config_.unchanged_field_ratio_threshold;

        if (want_dictionary != dictionary_enabled_ || want_delta != delta_enabled_)
        {
            changed = true;
        }

        dictionary_enabled_ = want_dictionary;
        delta_enabled_ = want_delta;

        previous_strings_[it->first] = stats.strings;
        stats = WindowStats {}; // keep last_payload empty: new window re-baselines
    }

    if (window_completed && (changed || !decisions_applied_))
    {
        apply_decisions(runtime);
        decisions_applied_ = true;
    }
}

void AdaptiveProfiler::apply_decisions(const SchemaRuntime& runtime)
{
    AdaptiveDictionaryConfig dictionary = runtime.dictionary_config();
    if (!runtime.dictionary_explicit())
    {
        dictionary.enabled = dictionary_enabled_;
    }

    RuntimeOptimizerConfig optimizer = runtime.optimizer_config();

    StreamOptimizerConfig stream = runtime.stream_optimizer_config();
    if (!runtime.stream_explicit())
    {
        stream.enable_delta_packets = delta_enabled_;
    }

    runtime.apply_optimization_settings(dictionary, optimizer, stream);
}

}
