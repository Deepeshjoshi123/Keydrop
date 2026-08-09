# Table I

**Comparison of Existing Telemetry Serialization Systems**

| Framework | Binary Format | Schema Support | Human Readable | Zero Copy | Runtime Optimization | Transport Independent | Telemetry Oriented | Cross Platform | Open Source | Language Neutral | Implementation Verified |
|---|---|---|---|---|---|---|---|---|---|---|---|
| JSON | No | Optional via JSON Schema | Yes | No | No | Yes | No | Yes | Open standard | Yes | Official docs |
| Protocol Buffers | Yes | Required .proto schema | No | No | Generated-code optimization | Yes | No | Yes | Yes | Yes | Official docs |
| MessagePack | Yes | No | No | No | No | Yes | No | Yes | Yes | Yes | Official docs |
| FlatBuffers | Yes | Required .fbs schema | No | Yes | No | Yes | No | Yes | Yes | Yes | Official docs |
| Cap'n Proto | Yes | Required schema | No | Yes | No | Yes | No | Yes | Yes | Yes | Official docs |
| Keydrop | Yes | Yes | No | No | Yes | Yes | Yes | Yes | Yes | No | Repository verified |

## Source Basis

Non-Keydrop entries are based on official documentation:

- JSON: https://www.json.org/json-en.html and https://json-schema.org/
- Protocol Buffers: https://protobuf.dev/overview/
- MessagePack: https://msgpack.org/ and https://github.com/msgpack/msgpack/blob/master/spec.md
- FlatBuffers: https://flatbuffers.dev/
- Cap'n Proto: https://capnproto.org/

Keydrop entries are based only on the repository implementation. See `verification_report.md`.
