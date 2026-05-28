#include <cassert>

#include "keydrop/schema/adaptive_dictionary.hpp"
#include "keydrop/schema/schema_runtime.hpp"

using namespace keydrop;

int main()
{
    AdaptiveDictionary dict;
    AdaptiveDictionaryConfig cfg;
    cfg.enabled = true;
    cfg.enable_string_values = true;
    cfg.max_entries = 2;
    dict.configure(cfg);

    const AdaptiveDictionaryResult miss = dict.lookup_id("sensor_01");
    assert(miss.code == AdaptiveDictionaryCode::miss);

    const AdaptiveDictionaryResult create_a = dict.create_or_get("sensor_01");
    assert(create_a.ok());
    assert(create_a.id == 1);

    const AdaptiveDictionaryResult hit_a = dict.lookup_id("sensor_01");
    assert(hit_a.ok());
    assert(hit_a.id == 1);

    const AdaptiveDictionaryResult create_b = dict.create_or_get("sensor_02");
    assert(create_b.ok());
    assert(dict.size() == 2);

    // Evicts oldest when full.
    const AdaptiveDictionaryResult create_c = dict.create_or_get("sensor_03");
    assert(create_c.ok());
    assert(dict.size() == 2);
    assert(dict.lookup_id("sensor_01").code == AdaptiveDictionaryCode::miss);
    assert(dict.lookup_id("sensor_03").ok());

    // Update + lookup_value
    assert(dict.update(create_b.id, "sensor_02_updated"));
    const AdaptiveDictionaryResult value_lookup = dict.lookup_value(create_b.id);
    assert(value_lookup.ok());
    assert(value_lookup.value == "sensor_02_updated");

    // Evict + reset
    assert(dict.evict(create_b.id));
    assert(dict.lookup_value(create_b.id).code == AdaptiveDictionaryCode::miss);
    dict.reset();
    assert(dict.size() == 0);

    // Runtime compression/decode correctness and hit behavior.
    SchemaRuntime runtime;
    const SchemaDef schema {
        "SensorData",
        19,
        {
            FieldDef {"temperature", FieldType::u8, 0, {}},
            FieldDef {"device_id", FieldType::string, 1, FieldConstraints {true, 32}},
        }
    };
    assert(runtime.register_schema(schema).ok());

    RuntimeOptimizerConfig optimizer_cfg;
    optimizer_cfg.enabled = false;
    runtime.set_optimizer_config(optimizer_cfg);

    AdaptiveDictionaryConfig dict_cfg;
    dict_cfg.enabled = true;
    dict_cfg.enable_string_values = true;
    dict_cfg.max_entries = 64;
    runtime.set_dictionary_config(dict_cfg);
    runtime.reset_dictionary();

    NamedPayload payload;
    payload["temperature"] = FieldValue::from_u8(10);
    payload["device_id"] = FieldValue::from_string("sensor_repeat");

    Buffer first_packet;
    assert(runtime.send("SensorData", payload, first_packet).ok());

    Buffer second_packet;
    assert(runtime.send("SensorData", payload, second_packet).ok());

    // Second packet should be smaller due to dictionary ID reference hit.
    assert(second_packet.size() < first_packet.size());

    std::string schema_name;
    NamedPayload decoded;
    assert(runtime.receive(second_packet, schema_name, decoded).ok());
    assert(schema_name == "SensorData");
    assert(decoded["temperature"].as_u8 == 10);
    assert(decoded["device_id"].as_string == "sensor_repeat");

    return 0;
}
