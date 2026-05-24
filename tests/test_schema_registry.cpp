#include <cassert>

#include "keydrop/schema/schema_registry.hpp"

using namespace keydrop;

int main()
{
    SchemaRegistry registry;

    const SchemaDef sensor_data {
        "SensorData",
        1,
        {
            FieldDef {"temperature", FieldType::u8, 0, {}},
            FieldDef {"humidity", FieldType::u8, 1, {}},
        }
    };

    const SchemaRegistryStatus register_ok = registry.register_schema(sensor_data);
    assert(register_ok.ok());
    assert(register_ok.code == SchemaRegistryStatusCode::ok);
    assert(registry.size() == 1);

    const SchemaDef* by_name = registry.find_by_name("SensorData");
    assert(by_name != nullptr);
    assert(by_name->message_id == 1);

    const SchemaDef* by_id = registry.find_by_message_id(1);
    assert(by_id != nullptr);
    assert(by_id->schema_name == "SensorData");

    // Duplicate name rejection
    const SchemaDef duplicate_name {
        "SensorData",
        2,
        {
            FieldDef {"device_id", FieldType::string, 0, {}},
        }
    };

    const SchemaRegistryStatus dup_name_status = registry.register_schema(duplicate_name);
    assert(!dup_name_status.ok());
    assert(dup_name_status.code == SchemaRegistryStatusCode::duplicate_name);
    assert(registry.size() == 1);

    // Duplicate message ID rejection
    const SchemaDef duplicate_id {
        "EnvData",
        1,
        {
            FieldDef {"pressure", FieldType::u32, 0, {}},
        }
    };

    const SchemaRegistryStatus dup_id_status = registry.register_schema(duplicate_id);
    assert(!dup_id_status.ok());
    assert(dup_id_status.code == SchemaRegistryStatusCode::duplicate_message_id);
    assert(registry.size() == 1);

    // Lookup failures
    const SchemaDef* missing_name = registry.find_by_name("Unknown");
    assert(missing_name == nullptr);

    const SchemaDef* missing_id = registry.find_by_message_id(99);
    assert(missing_id == nullptr);

    // Invalid schema checks
    const SchemaDef empty_name {
        "",
        5,
        {
            FieldDef {"temperature", FieldType::u8, 0, {}},
        }
    };

    const SchemaRegistryStatus empty_name_status = registry.register_schema(empty_name);
    assert(!empty_name_status.ok());
    assert(empty_name_status.code == SchemaRegistryStatusCode::invalid_schema);

    const SchemaDef empty_fields {
        "NoFields",
        6,
        {}
    };

    const SchemaRegistryStatus empty_fields_status = registry.register_schema(empty_fields);
    assert(!empty_fields_status.ok());
    assert(empty_fields_status.code == SchemaRegistryStatusCode::invalid_schema);

    return 0;
}
