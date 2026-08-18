#include <cassert>
#include <vector>
#include "keydrop/schema/schema_config.hpp"
using namespace keydrop;

int main() {
 // ── Valid YAML: aliases, stable field IDs, fingerprint ─────────
 const std::string yaml="keydrop: 1\n\nschemas:\n  Sensor:\n    id: 42\n    version: 1\n    profile: telemetry-balanced\n    fields:\n      - key: temperature\n        type: uint16\n      - key: device\n        type: string\n        max_length: 16\n";
 std::vector<ConfiguredSchema> schemas; assert(SchemaConfig::load_yaml(yaml,schemas).ok()); assert(schemas.size()==1); assert(schemas[0].field_ids.size()==2); assert(schemas[0].fingerprint != 0);

 // Field IDs are stable across loads and overrideable explicitly.
 std::vector<ConfiguredSchema> again; assert(SchemaConfig::load_yaml(yaml,again).ok()); assert(again[0].field_ids == schemas[0].field_ids);
 const std::string yaml_explicit="keydrop: 1\nschemas:\n  Sensor:\n    id: 42\n    fields:\n      - key: temperature\n        type: uint16\n        id: 777\n      - key: device\n        type: string\n";
 std::vector<ConfiguredSchema> explicit_ids; assert(SchemaConfig::load_yaml(yaml_explicit,explicit_ids).ok()); assert(explicit_ids[0].field_ids[0]==777); assert(explicit_ids[0].field_ids[0] != explicit_ids[0].field_ids[1]);

 // ── Invalid configurations are safely rejected ─────────────────
 std::vector<ConfiguredSchema> bad_cfg; // load_yaml clears its output even on failure, so never reuse a needed vector
 assert(!SchemaConfig::load_yaml("keydrop: 1\nschemas:\n  Bad:\n    id: 2\n    fields:\n      - key: a\n        type: timestamp_ms\n",bad_cfg).ok());              // unsupported type
 assert(!SchemaConfig::load_yaml("keydrop: 1\nschemas:\n  Bad:\n    id: 0\n    fields:\n      - key: a\n        type: u8\n",bad_cfg).ok());                       // id 0
 assert(!SchemaConfig::load_yaml("keydrop: 1\nschemas:\n  Bad:\n    id: 70000\n    fields:\n      - key: a\n        type: u8\n",bad_cfg).ok());                    // id > 65535
 assert(!SchemaConfig::load_yaml("keydrop: 1\nschemas:\n  Bad:\n    id: 3\n    fields:\n      - key: a\n        type: u8\n      - key: a\n        type: u8\n",bad_cfg).ok()); // duplicate field name
 assert(!SchemaConfig::load_yaml("keydrop: 1\nschemas:\n  Bad:\n    id: 4\n    fields:\n      - key: a\n        type: u8\n        scale: 100\n",bad_cfg).ok());       // unsupported option
 assert(!SchemaConfig::load_yaml("keydrop: 1\nschemas:\n  Bad:\n    id: 5\n    fields:\n      - key: a\n",bad_cfg).ok());                                        // field missing type
 assert(!SchemaConfig::load_yaml("keydrop: 1\nschemas:\n  Bad:\n    id: 6\n    fields:\n      - type: u8\n",bad_cfg).ok());                                        // field missing key
 assert(!SchemaConfig::load_yaml("keydrop: 1\n",bad_cfg).ok());                                                                                                    // no schemas
 assert(!SchemaConfig::load_yaml("schemas:\n  X:\n    id: 7\n",bad_cfg).ok());                                                                                      // missing keydrop marker

 // ── JSON round-trip: semantic correctness ──────────────────────
 JsonObject json; assert(parse_json_object("{\"temperature\":23,\"device\":\"one\"}",json).ok());
 SchemaRuntime runtime; assert(register_configured_schema(runtime,schemas[0]).ok());
 Buffer packet; assert(runtime.send_json("Sensor",json,packet).ok());
 std::string name; JsonObject decoded; assert(runtime.receive_json(packet,name,decoded).ok()); assert(name=="Sensor" && decoded["temperature"].integer_value==23 && decoded["device"].string_value=="one");

 // ── No stateless packet-size regression: JSON API bytes == named API bytes
 NamedPayload np; np["temperature"]=FieldValue::from_u16(23); np["device"]=FieldValue::from_string("one");
 Buffer named_packet; assert(runtime.send("Sensor",np,named_packet).ok());
 assert(named_packet.data() == packet.data());

 // ── Invalid payloads are safely rejected ───────────────────────
 JsonObject bad;
 bad=json; bad.erase("device");        assert(runtime.send_json("Sensor",bad,packet).code==SchemaRuntimeCode::json_conversion_failed); // missing field
 bad=json; bad["device"]=JsonValue::from_integer(5); assert(runtime.send_json("Sensor",bad,packet).code==SchemaRuntimeCode::json_conversion_failed); // wrong type
 bad=json; bad["temperature"]=JsonValue::from_integer(70000); assert(runtime.send_json("Sensor",bad,packet).code==SchemaRuntimeCode::json_conversion_failed); // range
 bad=json; bad["extra"]=JsonValue::from_integer(1); assert(runtime.send_json("Sensor",bad,packet).code==SchemaRuntimeCode::json_conversion_failed); // unknown field
 bad=json; bad["device"]=JsonValue::from_string("this_device_name_is_far_too_long"); assert(runtime.send_json("Sensor",bad,packet).code==SchemaRuntimeCode::schema_mismatch); // max_length
 assert(runtime.send_json("Unknown",json,packet).code==SchemaRuntimeCode::schema_not_found); // unknown schema

 // ── Version/fingerprint mismatch is rejected, never silently decoded
 auto peer=schemas[0]; peer.version=2; peer.fingerprint=SchemaConfig::fingerprint(peer); assert(!SchemaConfig::check_compatible(schemas[0],peer).ok());
 auto reordered=schemas[0]; reordered.version=1; std::swap(reordered.schema.fields[0],reordered.schema.fields[1]); reordered.fingerprint=SchemaConfig::fingerprint(reordered); assert(!SchemaConfig::check_compatible(schemas[0],reordered).ok());
 assert(SchemaConfig::check_compatible(schemas[0],schemas[0]).ok());

 // ── Generated example JSON and documentation cover every field ─
 const std::string example=SchemaConfig::generate_example_json(schemas[0]); JsonObject example_parsed; assert(parse_json_object(example,example_parsed).ok()); assert(example_parsed.count("temperature")==1 && example_parsed.count("device")==1);
 const std::string markdown=SchemaConfig::generate_markdown(schemas[0]); assert(markdown.find("temperature")!=std::string::npos && markdown.find("device")!=std::string::npos && markdown.find("Fingerprint")!=std::string::npos);
 return 0;
}
