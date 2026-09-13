# Phase 1: YAML Configuration and JSON API

Keydrop's beginner workflow uses one YAML file shared by the sender and
receiver. The CLI validates field names and types before it writes a binary
packet.

```bash
build/bin/keydrop_cli validate integration/configs/beginner_sensor.yaml
build/bin/keydrop_cli inspect integration/configs/beginner_sensor.yaml SensorReading
build/bin/keydrop_cli example integration/configs/beginner_sensor.yaml SensorReading
build/bin/keydrop_cli encode integration/configs/beginner_sensor.yaml SensorReading integration/configs/beginner_sensor.json
build/bin/keydrop_cli decode integration/configs/beginner_sensor.yaml <packet-hex>
build/bin/keydrop_cli compatibility local.yaml SensorReading peer.yaml SensorReading
```

`inspect` prints the generated configuration documentation (field table with
types, stable field IDs, constraints, and fingerprint). `example` prints a
generated example JSON payload containing every schema field, ready to edit
and pass to `encode`. `init --from-json payload.json MySchema out.yaml`
generates a starting YAML configuration from a sample JSON payload.

`compatibility` is the required fingerprint/version handshake before enabling
stateful dictionary or stream modes. A mismatch is rejected with the schema
name, ID, version, or fingerprint cause.

Supported YAML types are `u8`, `u16`, `u32`, `i8`, `i16`, `i32`, `f32`, `f64`,
`string`, and `bytes`, plus familiar aliases such as `uint16`. Strings are JSON
strings and bytes are JSON strings in hexadecimal (`"0x0a10"`). Each field can
set `max_length`; its field ID is stable from `schema-name:field-name` unless an
explicit positive `id` is supplied.

`timestamp_ms`, `decimal`, `delta`, `scale`, and `range` are deliberately
rejected today because their wire semantics are not yet implemented. This keeps
Phase 1 configurations safe rather than silently changing telemetry values.
