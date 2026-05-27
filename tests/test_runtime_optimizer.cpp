#include <cassert>

#include "keydrop/schema/schema_runtime.hpp"
#include "keydrop/schema/runtime_optimizer.hpp"

using namespace keydrop;

int main()
{
    SchemaRuntime runtime;

    const SchemaDef schema {
        "Telemetry",
        11,
        {
            FieldDef {"temperature", FieldType::u8, 0, {}},
            FieldDef {"humidity", FieldType::u16, 1, {}},
            FieldDef {"pressure", FieldType::u32, 2, {}},
            FieldDef {"device_id", FieldType::string, 3, FieldConstraints {true, 16}},
        }
    };
    assert(runtime.register_schema(schema).ok());

    NamedPayload payload;
    payload["temperature"] = FieldValue::from_u8(0);
    payload["humidity"] = FieldValue::from_u16(0);
    payload["pressure"] = FieldValue::from_u32(0);
    payload["device_id"] = FieldValue::from_string("s1");

    RuntimeOptimizerConfig disabled;
    disabled.enabled = false;
    runtime.set_optimizer_config(disabled);

    Buffer unoptimized_packet;
    assert(runtime.send("Telemetry", payload, unoptimized_packet).ok());
    assert(!RuntimeOptimizer::is_optimized_packet(unoptimized_packet));

    RuntimeOptimizerConfig enabled;
    enabled.enabled = true;
    enabled.enable_zero_value_omission = true;
    runtime.set_optimizer_config(enabled);

    Buffer optimized_packet;
    assert(runtime.send("Telemetry", payload, optimized_packet).ok());
    assert(RuntimeOptimizer::is_optimized_packet(optimized_packet));
    assert(optimized_packet.size() < unoptimized_packet.size());

    std::string schema_name;
    NamedPayload decoded;
    assert(runtime.receive(optimized_packet, schema_name, decoded).ok());
    assert(schema_name == "Telemetry");
    assert(decoded["temperature"].as_u8 == 0);
    assert(decoded["humidity"].as_u16 == 0);
    assert(decoded["pressure"].as_u32 == 0);
    assert(decoded["device_id"].as_string == "s1");

    // When savings are not possible, packet stays in original form.
    NamedPayload full_payload;
    full_payload["temperature"] = FieldValue::from_u8(1);
    full_payload["humidity"] = FieldValue::from_u16(2);
    full_payload["pressure"] = FieldValue::from_u32(3);
    full_payload["device_id"] = FieldValue::from_string("s1");

    Buffer non_zero_packet;
    assert(runtime.send("Telemetry", full_payload, non_zero_packet).ok());
    assert(!RuntimeOptimizer::is_optimized_packet(non_zero_packet));

    return 0;
}
