#pragma once

#include <string>
#include <vector>

#include "keydrop/schema/json_types.hpp"
#include "keydrop/schema/schema_runtime.hpp"

namespace keydrop {

// Phase 1's dependency-free, intentionally small YAML schema surface.
// It accepts the documented mapping/list syntax and rejects unsupported YAML
// constructs instead of silently interpreting them differently.
struct ConfiguredFieldDef {
    FieldDef field;
    u32 field_id = 0;
};

struct ConfiguredSchema {
    SchemaDef schema;
    u16 version = 1;
    std::string profile = "telemetry-balanced";
    std::vector<u32> field_ids;
    u64 fingerprint = 0;
};

enum class SchemaConfigCode {
    ok,
    io_error,
    syntax_error,
    missing_value,
    invalid_value,
    unsupported_feature,
    schema_invalid,
    compatibility_mismatch,
    json_invalid
};

struct SchemaConfigResult {
    SchemaConfigCode code = SchemaConfigCode::ok;
    std::string message;
    bool ok() const { return code == SchemaConfigCode::ok; }
};

class SchemaConfig {
public:
    static SchemaConfigResult load_yaml(const std::string& yaml, std::vector<ConfiguredSchema>& out);
    static SchemaConfigResult load_yaml_file(const std::string& path, std::vector<ConfiguredSchema>& out);
    static u32 stable_field_id(const std::string& schema_name, const std::string& field_name);
    static u64 fingerprint(const ConfiguredSchema& schema);
    static SchemaConfigResult check_compatible(const ConfiguredSchema& local, const ConfiguredSchema& peer);
    static std::string generate_example_json(const ConfiguredSchema& schema);
    static std::string generate_markdown(const ConfiguredSchema& schema);
};

// Text JSON is deliberately kept separate from JsonObject, which is the
// runtime's typed representation. Bytes use an "0x..." hexadecimal string.
SchemaConfigResult parse_json_object(const std::string& json, JsonObject& out);
std::string format_json_object(const JsonObject& object);

SchemaConfigResult register_configured_schema(SchemaRuntime& runtime, const ConfiguredSchema& schema);

} // namespace keydrop
