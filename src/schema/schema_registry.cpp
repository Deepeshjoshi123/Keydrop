#include "keydrop/schema/schema_registry.hpp"

namespace keydrop {

SchemaRegistryStatus SchemaRegistry::register_schema(const SchemaDef& schema)
{
    if (schema.schema_name.empty())
    {
        return {
            SchemaRegistryStatusCode::invalid_schema,
            "Schema name cannot be empty."
        };
    }

    if (schema.fields.empty())
    {
        return {
            SchemaRegistryStatusCode::invalid_schema,
            "Schema must contain at least one field."
        };
    }

    if (schemas_by_name_.find(schema.schema_name) != schemas_by_name_.end())
    {
        return {
            SchemaRegistryStatusCode::duplicate_name,
            "Schema name already registered: " + schema.schema_name
        };
    }

    if (name_by_message_id_.find(schema.message_id) != name_by_message_id_.end())
    {
        return {
            SchemaRegistryStatusCode::duplicate_message_id,
            "Schema message_id already registered: " + std::to_string(schema.message_id)
        };
    }

    schemas_by_name_.insert(std::make_pair(schema.schema_name, schema));
    name_by_message_id_.insert(std::make_pair(schema.message_id, schema.schema_name));

    return {
        SchemaRegistryStatusCode::ok,
        "Schema registered successfully."
    };
}

const SchemaDef* SchemaRegistry::find_by_name(const std::string& schema_name) const
{
    const auto it = schemas_by_name_.find(schema_name);
    if (it == schemas_by_name_.end())
    {
        return nullptr;
    }

    return &(it->second);
}

const SchemaDef* SchemaRegistry::find_by_message_id(u16 message_id) const
{
    const auto id_it = name_by_message_id_.find(message_id);
    if (id_it == name_by_message_id_.end())
    {
        return nullptr;
    }

    return find_by_name(id_it->second);
}

usize SchemaRegistry::size() const
{
    return schemas_by_name_.size();
}

void SchemaRegistry::clear()
{
    schemas_by_name_.clear();
    name_by_message_id_.clear();
}

}
