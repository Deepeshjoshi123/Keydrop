#pragma once

#include <string>
#include <utility>
#include <vector>

#include "keydrop/core/buffer.hpp"
#include "keydrop/schema/field_mapper.hpp"
#include "keydrop/schema/json_types.hpp"
#include "keydrop/schema/adaptive_dictionary.hpp"
#include "keydrop/schema/runtime_optimizer.hpp"
#include "keydrop/schema/schema_registry.hpp"
#include "keydrop/schema/schema_validator.hpp"
#include "keydrop/schema/stream_optimizer.hpp"

namespace keydrop {

enum class SchemaRuntimeCode {
    ok,
    schema_not_found,
    schema_invalid,
    schema_mismatch,
    mapping_failed,
    decode_failed,
    corruption_detected,
    synchronization_failed,
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

    SchemaRuntimeResult receive_as(
        const std::string& expected_schema_name,
        const Buffer& packet,
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

    SchemaRuntimeResult send_stream(
        const std::string& schema_name,
        const NamedPayload& payload,
        Buffer& out_packet,
        bool& out_has_packet
    ) const;

    SchemaRuntimeResult flush_stream(
        Buffer& out_packet,
        bool& out_has_packet
    ) const;

    SchemaRuntimeResult receive_stream(
        const Buffer& packet,
        std::vector<std::pair<std::string, NamedPayload>>& out_messages
    ) const;

    SchemaRuntimeResult receive_recovered_stream(
        const Buffer& stream,
        std::vector<std::pair<std::string, NamedPayload>>& out_messages,
        usize& out_skipped_bytes
    ) const;

    const SchemaRegistry& registry() const;
    void set_optimizer_config(const RuntimeOptimizerConfig& config);
    const RuntimeOptimizerConfig& optimizer_config() const;
    void set_dictionary_config(const AdaptiveDictionaryConfig& config);
    const AdaptiveDictionaryConfig& dictionary_config() const;
    void reset_dictionary();
    void set_stream_optimizer_config(const StreamOptimizerConfig& config);
    const StreamOptimizerConfig& stream_optimizer_config() const;
    void reset_stream_optimizer();

private:
    SchemaRegistry registry_;
    RuntimeOptimizerConfig optimizer_config_;
    mutable AdaptiveDictionary dictionary_;
    mutable StreamOptimizer stream_optimizer_;

    SchemaRuntimeResult receive_with_schema(
        const SchemaDef& schema,
        const Buffer& packet,
        std::string& out_schema_name,
        NamedPayload& out_payload
    ) const;
};

}
