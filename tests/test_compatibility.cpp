// Phase 5 compatibility test: the schema fingerprint/session handshake
// rejects every mismatch mode with a specific reason, and the documented
// evolution policy holds: any field-list change is wire-incompatible and
// must never be silently decoded.

#include <cassert>
#include <string>
#include <vector>

#include "keydrop/schema/schema_config.hpp"
#include "keydrop/schema/schema_runtime.hpp"

using namespace keydrop;

namespace {

const std::string kYaml =
    "keydrop: 1\nschemas:\n  Telemetry:\n    id: 42\n    version: 1\n"
    "    fields:\n      - key: temperature\n        type: uint16\n"
    "      - key: device\n        type: string\n        max_length: 16\n";

ConfiguredSchema load_one(const std::string& yaml)
{
    std::vector<ConfiguredSchema> schemas;
    assert(SchemaConfig::load_yaml(yaml, schemas).ok());
    assert(schemas.size() == 1);
    return schemas[0];
}

} // namespace

int main()
{
    const ConfiguredSchema local = load_one(kYaml);

    // ── Identical configurations are compatible ──────────────────
    assert(SchemaConfig::check_compatible(local, load_one(kYaml)).ok());

    // ── Every mismatch mode is rejected with a specific reason ────
    // Name differs.
    ConfiguredSchema peer = load_one(kYaml);
    peer.schema.schema_name = "Other";
    {
        const SchemaConfigResult r = SchemaConfig::check_compatible(local, peer);
        assert(!r.ok() && r.message.find("names differ") != std::string::npos);
    }

    // Message id differs.
    peer = load_one(kYaml);
    peer.schema.message_id = 43;
    {
        const SchemaConfigResult r = SchemaConfig::check_compatible(local, peer);
        assert(!r.ok() && r.message.find("IDs differ") != std::string::npos);
    }

    // Version differs.
    peer = load_one(kYaml);
    peer.version = 2;
    peer.fingerprint = SchemaConfig::fingerprint(peer);
    {
        const SchemaConfigResult r = SchemaConfig::check_compatible(local, peer);
        assert(!r.ok() && r.message.find("version mismatch") != std::string::npos);
    }

    // Field order differs (same fields, different wire layout).
    peer = load_one(kYaml);
    std::swap(peer.schema.fields[0], peer.schema.fields[1]);
    peer.fingerprint = SchemaConfig::fingerprint(peer);
    {
        const SchemaConfigResult r = SchemaConfig::check_compatible(local, peer);
        assert(!r.ok() && r.message.find("fingerprint mismatch") != std::string::npos);
    }

    // Field type differs.
    peer = load_one(kYaml);
    peer.schema.fields[0].type = FieldType::u32;
    peer.fingerprint = SchemaConfig::fingerprint(peer);
    {
        const SchemaConfigResult r = SchemaConfig::check_compatible(local, peer);
        assert(!r.ok() && r.message.find("fingerprint mismatch") != std::string::npos);
    }

    // Constraint differs.
    peer = load_one(kYaml);
    peer.schema.fields[1].constraints.max_length = 8;
    peer.fingerprint = SchemaConfig::fingerprint(peer);
    {
        const SchemaConfigResult r = SchemaConfig::check_compatible(local, peer);
        assert(!r.ok() && r.message.find("fingerprint mismatch") != std::string::npos);
    }

    // ── Evolution policy: appended field requires a version bump and is
    // wire-incompatible — never silently decoded ──────────────────
    const ConfiguredSchema evolved = load_one(
        "keydrop: 1\nschemas:\n  Telemetry:\n    id: 42\n    version: 1\n"
        "    fields:\n      - key: temperature\n        type: uint16\n"
        "      - key: device\n        type: string\n        max_length: 16\n"
        "      - key: humidity\n        type: uint16\n");
    assert(!SchemaConfig::check_compatible(local, evolved).ok());

    // ── Handshake gates the stateful stream: a compatible peer streams
    // successfully; a version-mismatched peer is rejected before use ──
    SchemaRuntime sender;
    assert(register_configured_schema(sender, local).ok());
    assert(apply_configured_profile(sender, local).ok());

    NamedPayload payload;
    payload["temperature"] = FieldValue::from_u16(23);
    payload["device"] = FieldValue::from_string("sensor-01");

    Buffer packet;
    assert(sender.send("Telemetry", payload, packet).ok());

    SchemaRuntime matching_receiver;
    assert(register_configured_schema(matching_receiver, load_one(kYaml)).ok());
    std::string schema_name;
    NamedPayload decoded;
    assert(matching_receiver.receive(packet, schema_name, decoded).ok());
    assert(decoded["temperature"].as_u16 == 23);

    // Unknown message_id is rejected, never decoded.
    Buffer foreign;
    foreign.write(0x63);
    foreign.write(0x00); // message_id 99
    assert(matching_receiver.receive(foreign, schema_name, decoded).code == SchemaRuntimeCode::schema_not_found);

    // Unknown schema name is rejected.
    assert(matching_receiver.receive_as("DoesNotExist", packet, decoded).code == SchemaRuntimeCode::schema_not_found);

    // Message id mismatch on an expected schema is rejected.
    assert(matching_receiver.receive_as("Telemetry", foreign, decoded).code == SchemaRuntimeCode::schema_mismatch);

    return 0;
}
