/// 05-corruption-recovery.cpp — Reliability: packet recovery from noise
///
/// What you learn:
///  1. Keydrop can recover valid packets from a corrupted byte stream
///  2. PacketSynchronizer scans for valid packet boundaries
///  3. Skipped (corrupted) bytes are reported so you can log/monitor them
///
/// Build:  see CMakeLists.txt in this directory
/// Run:    ./build/integration/05-corruption-recovery

#include <cassert>
#include <iostream>
#include <string>

#include "keydrop/schema/schema_runtime.hpp"

using namespace keydrop;

int main()
{
    // ── 1. Register schemas ────────────────────────────────────
    SchemaRuntime runtime;
    runtime.register_schema(SchemaDef{"Telemetry", 99, {
        FieldDef{"sensor_id", FieldType::u16,    0, {}},
        FieldDef{"reading",   FieldType::u32,    1, {}},
    }});

    // ── 2. Encode a few valid packets ──────────────────────────
    Buffer good1, good2;
    {
        NamedPayload p;
        p["sensor_id"] = FieldValue::from_u16(1);
        p["reading"]   = FieldValue::from_u32(100);
        runtime.send("Telemetry", p, good1);
    }
    {
        NamedPayload p;
        p["sensor_id"] = FieldValue::from_u16(2);
        p["reading"]   = FieldValue::from_u32(200);
        runtime.send("Telemetry", p, good2);
    }

    // ── 3. Build a corrupted stream: garbage + valid + garbage + valid
    Buffer corrupted;
    byte noise[] = {0xFF, 0x00, 0xAA, 0x55, 0xFF};   // 5 bytes of noise
    corrupted.append(noise, sizeof(noise));            // garbage prefix
    corrupted.append(good1);                           // valid packet
    corrupted.append(noise, 3);                        // more noise
    corrupted.append(good2);                           // valid packet

    std::cout << "Corrupted stream: " << corrupted.size() << " bytes\n";
    std::cout << "  " << sizeof(noise) << " B noise + valid + 3 B noise + valid\n\n";

    // ── 4. Recover all valid packets ───────────────────────────
    std::vector<std::pair<std::string, NamedPayload>> messages;
    usize skipped_bytes = 0;
    SchemaRuntimeResult res = runtime.receive_recovered_stream(
        corrupted, messages, skipped_bytes);

    assert(res.ok());
    std::cout << "[recovery] skipped " << skipped_bytes
              << " corrupted bytes\n";
    std::cout << "[recovery] recovered " << messages.size()
              << " valid packets:\n";

    for (size_t i = 0; i < messages.size(); ++i) {
        const auto& msg = messages[i];
        std::cout << "  [" << i << "] schema=" << msg.first
                  << "  sensor_id=" << msg.second.at("sensor_id").as_u16
                  << "  reading="   << msg.second.at("reading").as_u32 << "\n";
    }

    // ── 5. Verify recovered values ─────────────────────────────
    assert(messages.size() == 2);
    assert(messages[0].second.at("sensor_id").as_u16 == 1);
    assert(messages[1].second.at("sensor_id").as_u16 == 2);
    assert(skipped_bytes == 5 + 3);  // 5 prefix + 3 mid-stream

    std::cout << "\n[pass] recovered both packets from corrupted stream\n";
    return 0;
}
