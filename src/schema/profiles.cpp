#include "keydrop/schema/profiles.hpp"

#include "keydrop/schema/schema_runtime.hpp"

namespace keydrop {

bool try_get_profile(const std::string& name, ProfileSettings& out)
{
    if (name == "telemetry-low-latency")
    {
        ProfileSettings profile;
        profile.name = name;
        profile.dictionary = AdaptiveDictionaryConfig {false, true, 256};
        profile.runtime = RuntimeOptimizerConfig {false, true};
        profile.stream = StreamOptimizerConfig {};
        profile.adaptive = false;
        out = profile;
        return true;
    }

    if (name == "telemetry-balanced")
    {
        ProfileSettings profile;
        profile.name = name;
        profile.dictionary = AdaptiveDictionaryConfig {true, true, 256};
        profile.runtime = RuntimeOptimizerConfig {true, true};
        profile.stream.enabled = true;
        profile.stream.enable_packet_reuse = true;
        profile.stream.enable_delta_updates = true;
        profile.stream.enable_batching = true;
        profile.stream.enable_delta_packets = false;
        profile.stream.keyframe_interval = 100;
        profile.stream.aggressive_after_samples = 8;
        profile.stream.max_batch_packets = 4;
        profile.stream.low_change_ratio_threshold = 0.35f;
        profile.adaptive = true;
        out = profile;
        return true;
    }

    if (name == "telemetry-bandwidth")
    {
        ProfileSettings profile;
        profile.name = name;
        profile.dictionary = AdaptiveDictionaryConfig {true, true, 256};
        profile.runtime = RuntimeOptimizerConfig {true, true};
        profile.stream.enabled = true;
        profile.stream.enable_packet_reuse = true;
        profile.stream.enable_delta_updates = true;
        profile.stream.enable_batching = false;
        profile.stream.enable_delta_packets = true;
        profile.stream.keyframe_interval = 100;
        profile.stream.aggressive_after_samples = 1;
        profile.stream.low_change_ratio_threshold = 0.5f;
        profile.adaptive = true;
        out = profile;
        return true;
    }

    if (name == "telemetry-lossless-archive")
    {
        ProfileSettings profile;
        profile.name = name;
        profile.dictionary = AdaptiveDictionaryConfig {true, true, 256};
        profile.runtime = RuntimeOptimizerConfig {true, true};
        profile.stream.enabled = true;
        profile.stream.enable_packet_reuse = true;
        profile.stream.enable_delta_updates = true;
        profile.stream.enable_batching = true;
        profile.stream.enable_delta_packets = true;
        profile.stream.keyframe_interval = 50; // shorter recovery window
        profile.stream.aggressive_after_samples = 1;
        profile.stream.max_batch_packets = 4;
        profile.stream.low_change_ratio_threshold = 0.5f;
        profile.adaptive = true;
        out = profile;
        return true;
    }

    return false;
}

void apply_profile(SchemaRuntime& runtime, const ProfileSettings& profile)
{
    runtime.apply_optimization_settings(profile.dictionary, profile.runtime, profile.stream);
}

}
