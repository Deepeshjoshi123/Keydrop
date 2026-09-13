/// 02-with-dictionary.cpp — Adaptive string dictionary
///
/// What you learn:
///  1. Enable the adaptive dictionary on the runtime
///  2. Repeated strings get replaced with a 2-byte ID on the wire
///  3. The dictionary learns automatically — no manual config per message
///
/// Build:  see CMakeLists.txt in this directory
/// Run:    ./build/integration/02-with-dictionary

#include <cassert>
#include <iostream>
#include <string>

#include "keydrop/schema/schema_runtime.hpp"

using namespace keydrop;

int main()
{
    // ── 1. Schema + runtime with dictionary enabled ────────────
    SchemaDef schema{"DeviceLog", 50, {
        FieldDef{"level",   FieldType::u8,     0, {}},
        FieldDef{"message", FieldType::string, 1, FieldConstraints{true, 128}},
    }};

    SchemaRuntime runtime;
    runtime.register_schema(schema);

    AdaptiveDictionaryConfig dict_cfg;
    dict_cfg.enabled             = true;
    dict_cfg.enable_string_values = true;
    dict_cfg.max_entries         = 256;
    runtime.set_dictionary_config(dict_cfg);

    // ── 2. First send — full string transmitted ────────────────
    NamedPayload p1;
    p1["level"]   = FieldValue::from_u8(1);
    p1["message"] = FieldValue::from_string("temperature spike");

    Buffer first_packet;
    runtime.send("DeviceLog", p1, first_packet);
    std::cout << "[1st send]  packet size: " << first_packet.size() << " B"
              << "  (string sent in full)\n";

    // ── 3. Second send — string replaced by 2-byte dictionary ID
    NamedPayload p2;
    p2["level"]   = FieldValue::from_u8(2);
    p2["message"] = FieldValue::from_string("temperature spike");   // same string

    Buffer second_packet;
    runtime.send("DeviceLog", p2, second_packet);
    std::cout << "[2nd send]  packet size: " << second_packet.size() << " B"
              << "  (string replaced by 2-byte ID)\n";

    assert(second_packet.size() < first_packet.size());
    std::cout << "            saved " << (first_packet.size() - second_packet.size())
              << " bytes via dictionary reuse\n";

    // ── 4. Both decode back to the same value ──────────────────
    std::string schema_name;
    NamedPayload decoded1, decoded2;
    runtime.receive(first_packet,  schema_name, decoded1);
    runtime.receive(second_packet, schema_name, decoded2);

    assert(decoded1["message"].as_string == "temperature spike");
    assert(decoded2["message"].as_string == "temperature spike");
    std::cout << "\n[pass] both packets decoded correctly\n";

    return 0;
}
