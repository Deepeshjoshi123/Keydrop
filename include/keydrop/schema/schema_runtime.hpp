#pragma once

#include <string>

#include "keydrop/core/buffer.hpp"
#include "keydrop/schema/field_mapper.hpp"
#include "keydrop/schema/json_types.hpp"
#include "keydrop/schema/adaptive_dictionary.hpp"
#include "keydrop/schema/runtime_optimizer.hpp"
#include "keydrop/schema/schema_registry.hpp"
#include "keydrop/schema/schema_validator.hpp"

namespace keydrop {

enum class SchemaRuntimeCode {
    ok,
    schema_not_found,
    schema_invalid,
    mapping_failed,
    decode_failed,
    packet_too_small,
    trailing_packet_data,
    json_conversion_failed
};

struct SchemaRuntimeResult {
    SchemaRuntimeCode code = SchemaRuntimeCode::ok;
    std::string message;

    bool ok() const
    {
        return code == SchemaRuntimeCode::ok;
    }
};

class SchemaRuntime {
public:
    SchemaRuntime() = default;

    SchemaRegistryStatus register_schema(const SchemaDef& schema);

    SchemaRuntimeResult send(
        const std::string& schema_name,
        const NamedPayload& payload,
        Buffer& out_packet
    ) const;

    SchemaRuntimeResult receive(
        const Buffer& packet,
        std::string& out_schema_name,
        NamedPayload& out_payload
    ) const;

    SchemaRuntimeResult send_json(
        const std::string& schema_name,
        const JsonObject& json_payload,
        Buffer& out_packet
    ) const;

    SchemaRuntimeResult receive_json(
        const Buffer& packet,
        std::string& out_schema_name,
        JsonObject& out_json_payload
    ) const;

    const SchemaRegistry& registry() const;
    void set_optimizer_config(const RuntimeOptimizerConfig& config);
    const RuntimeOptimizerConfig& optimizer_config() const;
    void set_dictionary_config(const AdaptiveDictionaryConfig& config);
    const AdaptiveDictionaryConfig& dictionary_config() const;
    void reset_dictionary();

private:
    SchemaRegistry registry_;
    RuntimeOptimizerConfig optimizer_config_;
    mutable AdaptiveDictionary dictionary_;
};

}
