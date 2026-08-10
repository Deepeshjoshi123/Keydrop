/// 01-send-receive.cpp — Minimal Keydrop integration
///
/// What you learn:
///  1. Define a telemetry schema in code
///  2. Register it with the runtime
///  3. Encode a payload into a binary packet  (send)
///  4. Decode the packet back into named fields (receive)
///
/// Build:  see CMakeLists.txt in this directory
/// Run:    ./build/integration/01-send-receive

#include <cassert>
#include <iostream>
#include <string>

#include "keydrop/schema/schema_runtime.hpp"

using namespace keydrop;

int main()
{
    // ── 1. Define a schema ─────────────────────────────────────
    SchemaDef sensor_schema{
        "SensorReading",           // human-readable name
        42,                        // unique message id  (1–65535)
        {
            FieldDef{"temperature",  FieldType::u16,    0, {}},
            FieldDef{"humidity",     FieldType::u16,    1, {}},
            FieldDef{"device_id",    FieldType::string, 2, FieldConstraints{true, 64}},
        }
    };

    // ── 2. Create the runtime and register ─────────────────────
    SchemaRuntime runtime;
    SchemaRegistryStatus reg = runtime.register_schema(sensor_schema);
    assert(reg.ok() && "schema registration failed");
    std::cout << "[ok] schema registered: " << sensor_schema.schema_name << "\n";

    // ── 3. Build a payload ─────────────────────────────────────
    NamedPayload payload;
    payload["temperature"] = FieldValue::from_u16(23);
    payload["humidity"]    = FieldValue::from_u16(71);
    payload["device_id"]   = FieldValue::from_string("sensor-01");

    // ── 4. Encode (send path) ──────────────────────────────────
    Buffer packet;
    SchemaRuntimeResult send_res = runtime.send("SensorReading", payload, packet);
    assert(send_res.ok());
    std::cout << "[ok] encoded packet: " << packet.size() << " bytes\n";

    // ── 5. Decode (receive path) ───────────────────────────────
    std::string decoded_schema;
    NamedPayload decoded;
    SchemaRuntimeResult recv_res = runtime.receive(packet, decoded_schema, decoded);
    assert(recv_res.ok());
    assert(decoded_schema == "SensorReading");

    std::cout << "[ok] decoded from schema: " << decoded_schema << "\n";
    std::cout << "     temperature = " << decoded["temperature"].as_u16 << "\n";
    std::cout << "     humidity    = " << decoded["humidity"].as_u16    << "\n";
    std::cout << "     device_id   = " << decoded["device_id"].as_string << "\n";

    // ── 6. Round-trip check ────────────────────────────────────
    assert(decoded["temperature"].as_u16  == payload["temperature"].as_u16);
    assert(decoded["humidity"].as_u16     == payload["humidity"].as_u16);
    assert(decoded["device_id"].as_string == payload["device_id"].as_string);
    std::cout << "\n[pass] round-trip verified — all fields match\n";

    return 0;
}
