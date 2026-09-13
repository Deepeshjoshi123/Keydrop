#include <cassert>
#include <string>
#include <vector>

#include "keydrop/schema/profiles.hpp"
#include "keydrop/schema/schema_config.hpp"
#include "keydrop/schema/schema_runtime.hpp"

using namespace keydrop;

namespace {

const SchemaDef kTelemetry {
    "Telemetry",
    70,
    {
        FieldDef {"device_id", FieldType::string, 0, FieldConstraints {true, 32}},
        FieldDef {"status", FieldType::string, 1, FieldConstraints {true, 32}},
        FieldDef {"timestamp", FieldType::u32, 2, {}},
        FieldDef {"temperature", FieldType::u16, 3, {}},
    }
};

SchemaRuntime make_runtime()
{
    SchemaRuntime runtime;
    assert(runtime.register_schema(kTelemetry).ok());
    return runtime;
}

// Suitable stream: constant strings, slowly changing values.
NamedPayload suitable(usize i)
{
    NamedPayload p;
    p["device_id"] = FieldValue::from_string("sensor_01");
    p["status"] = FieldValue::from_string("operational");
    p["timestamp"] = FieldValue::from_u32(static_cast<u32>(1000000 + i * 1000));
    p["temperature"] = FieldValue::from_u16(static_cast<u16>(210 + (i % 70) / 10));
    return p;
}

// Unsuitable stream: unique strings, every field changes.
NamedPayload unsuitable(usize i)
{
    NamedPayload p;
    p["device_id"] = FieldValue::from_string("device-" + std::to_string(i) + "-unique");
    p["status"] = FieldValue::from_string("state-" + std::to_string(i));
    p["timestamp"] = FieldValue::from_u32(static_cast<u32>(i * 997 + 13));
    p["temperature"] = FieldValue::from_u16(static_cast<u16>(i * 31 % 60000));
    return p;
}

} // namespace

int main()
{
    // ── Profiles: all four exist, unknown names rejected ─────────
    ProfileSettings low_latency;
    ProfileSettings balanced;
    ProfileSettings bandwidth;
    ProfileSettings archive;
    assert(try_get_profile("telemetry-low-latency", low_latency));
    assert(try_get_profile("telemetry-balanced", balanced));
    assert(try_get_profile("telemetry-bandwidth", bandwidth));
    assert(try_get_profile("telemetry-lossless-archive", archive));
    assert(!try_get_profile("not-a-profile", low_latency));
    assert(balanced.adaptive && bandwidth.adaptive && archive.adaptive && !low_latency.adaptive);
    assert(!low_latency.stream.enabled);
    assert(bandwidth.stream.enable_delta_packets && !balanced.stream.enable_delta_packets);
    assert(archive.stream.keyframe_interval < bandwidth.stream.keyframe_interval);

    // ── YAML rejects unknown profiles ────────────────────────────
    std::vector<ConfiguredSchema> schemas;
    assert(!SchemaConfig::load_yaml(
        "keydrop: 1\nschemas:\n  X:\n    id: 9\n    profile: telemetry-ultra\n    fields:\n      - key: a\n        type: u8\n",
        schemas).ok());

    // ── apply_profile + apply_configured_profile set runtime configs ──
    SchemaRuntime runtime = make_runtime();
    apply_profile(runtime, bandwidth);
    assert(runtime.dictionary_config().enabled);
    assert(runtime.stream_optimizer_config().enabled);
    assert(runtime.stream_optimizer_config().enable_delta_packets);
    assert(runtime.optimizer_config().enabled);

    std::vector<ConfiguredSchema> configured;
    assert(SchemaConfig::load_yaml(
        "keydrop: 1\nschemas:\n  Telemetry:\n    id: 70\n    profile: telemetry-bandwidth\n    fields:\n      - key: a\n        type: u8\n",
        configured).ok());
    SchemaRuntime yaml_runtime = make_runtime();
    assert(apply_configured_profile(yaml_runtime, configured[0]).ok());
    assert(yaml_runtime.stream_optimizer_config().enable_delta_packets);
    assert(yaml_runtime.adaptive_config().enabled);

    // ── Adaptive: suitable stream enables dictionary + deltas ────
    SchemaRuntime suitable_runtime = make_runtime();
    AdaptiveProfilerConfig adaptive;
    adaptive.enabled = true;
    adaptive.window_size = 10;
    suitable_runtime.set_adaptive_config(adaptive);

    for (usize i = 0; i < 10; ++i)
    {
        Buffer out;
        assert(suitable_runtime.send("Telemetry", suitable(i), out).ok());
    }
    assert(suitable_runtime.dictionary_config().enabled);
    assert(suitable_runtime.stream_optimizer_config().enable_delta_packets);

    // Lossless reconstruction through the adaptive stream.
    for (usize i = 10; i < 40; ++i)
    {
        Buffer out;
        bool has = false;
        assert(suitable_runtime.send_stream("Telemetry", suitable(i), out, has).ok());
        if (!has)
        {
            continue;
        }
        std::vector<std::pair<std::string, NamedPayload>> messages;
        assert(suitable_runtime.receive_stream(out, messages).ok());
        for (usize m = 0; m < messages.size(); ++m)
        {
            assert(messages[m].second["device_id"].as_string == "sensor_01");
        }
    }

    // ── Adaptive: unsuitable stream keeps dictionary and deltas off ──
    SchemaRuntime unsuitable_runtime = make_runtime();
    unsuitable_runtime.set_adaptive_config(adaptive);

    apply_profile(unsuitable_runtime, balanced); // start from a dict-on profile
    assert(unsuitable_runtime.dictionary_config().enabled);

    for (usize i = 0; i < 10; ++i)
    {
        Buffer out;
        assert(unsuitable_runtime.send("Telemetry", unsuitable(i), out).ok());
    }
    assert(!unsuitable_runtime.dictionary_config().enabled);
    assert(!unsuitable_runtime.stream_optimizer_config().enable_delta_packets);

    // ── Explicit user override always wins ───────────────────────
    SchemaRuntime override_runtime = make_runtime();
    override_runtime.set_adaptive_config(adaptive);
    AdaptiveDictionaryConfig explicit_dict;
    explicit_dict.enabled = false;
    override_runtime.set_dictionary_config(explicit_dict); // explicit
    StreamOptimizerConfig explicit_stream;
    explicit_stream.enabled = false;
    override_runtime.set_stream_optimizer_config(explicit_stream); // explicit

    for (usize i = 0; i < 10; ++i)
    {
        Buffer out;
        assert(override_runtime.send("Telemetry", suitable(i), out).ok());
    }
    assert(!override_runtime.dictionary_config().enabled); // adaptive must not override
    assert(!override_runtime.stream_optimizer_config().enabled);
    assert(!override_runtime.stream_optimizer_config().enable_delta_packets);

    // ── Reset clears decisions; a new window re-decides ──────────
    SchemaRuntime redecide_runtime = make_runtime();
    redecide_runtime.set_adaptive_config(adaptive);
    for (usize i = 0; i < 10; ++i)
    {
        Buffer out;
        assert(redecide_runtime.send("Telemetry", unsuitable(i), out).ok());
    }
    assert(!redecide_runtime.dictionary_config().enabled); // unsuitable window → off

    redecide_runtime.reset_adaptive_profiler();
    for (usize i = 0; i < 10; ++i)
    {
        Buffer out;
        assert(redecide_runtime.send("Telemetry", suitable(i), out).ok());
    }
    assert(redecide_runtime.dictionary_config().enabled); // suitable window → on again
    assert(redecide_runtime.stream_optimizer_config().enable_delta_packets);

    return 0;
}
