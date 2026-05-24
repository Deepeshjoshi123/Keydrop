#pragma once

#include <string>
#include <unordered_map>

#include "keydrop/schema/schema_types.hpp"

namespace keydrop {

enum class SchemaRegistryStatusCode {
    ok,
    duplicate_name,
    duplicate_message_id,
    invalid_schema
};

struct SchemaRegistryStatus {
    SchemaRegistryStatusCode code = SchemaRegistryStatusCode::ok;
    std::string message;

    bool ok() const
    {
        return code == SchemaRegistryStatusCode::ok;
    }
};

class SchemaRegistry {
public:
    SchemaRegistry() = default;

    SchemaRegistryStatus register_schema(const SchemaDef& schema);

    const SchemaDef* find_by_name(const std::string& schema_name) const;
    const SchemaDef* find_by_message_id(u16 message_id) const;

    usize size() const;
    void clear();

private:
    std::unordered_map<std::string, SchemaDef> schemas_by_name_;
    std::unordered_map<u16, std::string> name_by_message_id_;
};

}
