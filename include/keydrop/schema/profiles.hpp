#pragma once

#include <string>

#include "keydrop/schema/adaptive_dictionary.hpp"
#include "keydrop/schema/runtime_optimizer.hpp"
#include "keydrop/schema/stream_optimizer.hpp"

namespace keydrop {

class SchemaRuntime;

// Named configuration profiles (Phase 4). A profile is a starting point:
// the AdaptiveProfiler may refine it at runtime unless the user explicitly
// configured the corresponding component.
struct ProfileSettings {
    std::string name;
    AdaptiveDictionaryConfig dictionary;
    RuntimeOptimizerConfig runtime;
    StreamOptimizerConfig stream;
    bool adaptive = false;
};

bool try_get_profile(const std::string& name, ProfileSettings& out);

// Applies the profile to the runtime. Does not mark components as
// explicitly configured, so adaptive refinement may still adjust them.
void apply_profile(SchemaRuntime& runtime, const ProfileSettings& profile);

}
