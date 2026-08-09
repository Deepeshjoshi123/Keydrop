# Verification Report for Table I

This report documents how Keydrop entries in `comparison_table.csv` were marked. The Keydrop row uses only repository evidence; no unverified feature claims are included.

## Keydrop Table Row

| Table Column | Marking | Repository Evidence |
|---|---:|---|
| Binary Format | Yes | `SchemaRuntime::send` writes a `message_id` and encoded field bytes in `src/schema/schema_runtime.cpp:327`; `Encoder` writes fixed-width numeric values and length-prefixed strings/bytes in `include/keydrop/core/encoder.hpp` and `src/core/encoder.cpp`; `PacketLayout` describes binary field layout in `include/keydrop/schema/packet_layout.hpp`. |
| Schema Support | Yes | `SchemaDef` is implemented in `include/keydrop/schema/schema_types.hpp:35`; registration and lookup are implemented by `SchemaRegistry` in `include/keydrop/schema/schema_registry.hpp:28` and `src/schema/schema_registry.cpp:5`. |
| Human Readable | No | Runtime packets are produced through binary `Encoder`/`PacketBuilder` and decoded by `PacketReader`; JSON support is a conversion API (`send_json`, `receive_json`) in `src/schema/schema_runtime.cpp`, not the packet wire format. |
| Zero Copy | No | `PacketReader::read_string` returns `std::string` by value and `PacketReader::read_bytes` returns `std::vector<byte>` by value in `src/core/packet_reader.cpp`; `SchemaRuntime::receive_with_schema` decodes into `FieldValue` payload objects in `src/schema/schema_runtime.cpp:454`. `BufferView` exists in `include/keydrop/core/buffer.hpp`, but the schema decode path does not expose zero-copy typed access to serialized fields. |
| Runtime Optimization | Yes | `RuntimeOptimizer` is implemented in `include/keydrop/schema/runtime_optimizer.hpp:19` and `src/schema/runtime_optimizer.cpp:39`; `SchemaRuntime::send` invokes `RuntimeOptimizer::optimize_packet` in `src/schema/schema_runtime.cpp:385`. |
| Transport Independent | Yes | Serialization/runtime APIs use `Buffer` and are separate from the abstract `Transport` interface in `include/keydrop/transport/transport.hpp:65`; concrete TCP/WebSocket adapters are separate classes. |
| Telemetry Oriented | Yes | README describes Keydrop as an adaptive real-time telemetry transport engine and lists telemetry packet overhead, low-latency communication, and telemetry scheduling goals in `README.md`; implementation includes telemetry-oriented stream/runtime optimization in `src/schema/stream_optimizer.cpp`. |
| Cross Platform | Yes | README states Linux and Windows build support; platform socket code branches on `_WIN32` and POSIX in `src/platform/socket.cpp:3` and `src/transport/tcp_adapter.cpp:9`. |
| Open Source | Yes | README lists `MIT License` in `README.md`. |
| Language Neutral | No | The repository implements a C++17 library/API; no generated bindings or language-neutral schema compiler were found in `include/`, `src/`, `CMakeLists.txt`, or `README.md`. |
| Implementation Verified | Repository verified | All Keydrop table entries above are tied to source files in this repository. |

## Feature-by-Feature Repository Analysis

| Feature | Status | Evidence |
|---|---:|---|
| Schema Definition | Yes | `SchemaDef`, `FieldDef`, `FieldType`, and `FieldConstraints` in `include/keydrop/schema/schema_types.hpp:35`. |
| Schema Registry | Yes | `SchemaRegistry` exposes `register_schema`, `find_by_name`, and `find_by_message_id` in `include/keydrop/schema/schema_registry.hpp:28`; implementation starts at `src/schema/schema_registry.cpp:5`. |
| Schema Validation | Yes | `SchemaValidator` exposes schema, payload type, message ID, and payload value validation in `include/keydrop/schema/schema_validator.hpp:38`; implementations start at `src/schema/schema_validator.cpp:8` and `src/schema/schema_validator.cpp:114`. |
| Runtime Encoding | Yes | `SchemaRuntime::send` maps named payloads, validates them, writes message ID and fields, and returns a packet in `src/schema/schema_runtime.cpp:327`. |
| Runtime Decoding | Yes | `SchemaRuntime::receive` and `receive_with_schema` resolve schema, deoptimize if needed, validate, decode fields, and map ordered payloads to named payloads in `src/schema/schema_runtime.cpp:396` and `src/schema/schema_runtime.cpp:454`. |
| Packet Builder | Yes | `PacketBuilder` is declared in `include/keydrop/core/packet_builder.hpp:10` and implemented in `src/core/packet_builder.cpp`. |
| Packet Reader | Yes | `PacketReader` is declared in `include/keydrop/core/packet_reader.hpp:11` and implemented in `src/core/packet_reader.cpp`. |
| Binary Packet Format | Yes | `Encoder` writes little-endian numeric values and length-prefixed strings/bytes in `src/core/encoder.cpp`; `PacketLayout` records field codecs and sizes in `include/keydrop/schema/packet_layout.hpp`. |
| Transport Abstraction | Yes | Abstract `Transport` class and `TransportKind` are defined in `include/keydrop/transport/transport.hpp:65`; `TcpAdapter` and `WebSocketAdapter` implement it. |
| Cross Platform Networking | Yes | `_WIN32`/Winsock and POSIX socket branches appear in `src/platform/socket.cpp:3` and `src/transport/tcp_adapter.cpp:9`. |
| Runtime Optimization | Yes | `RuntimeOptimizer::optimize_packet`, `deoptimize_packet`, and `is_optimized_packet` are implemented in `src/schema/runtime_optimizer.cpp:39`, `src/schema/runtime_optimizer.cpp:138`, and `src/schema/runtime_optimizer.cpp:235`. |
| Packet Validation | Yes | `CorruptionDetector::check_keydrop_packet` is declared in `include/keydrop/reliability/corruption_detector.hpp:51` and called by `SchemaRuntime::receive_with_schema` before decoding. |
| Reliability Layer | Yes | `CorruptionDetector` and `PacketSynchronizer` are implemented under `include/keydrop/reliability/` and `src/reliability/`; recovered stream receive is exposed by `SchemaRuntime::receive_recovered_stream` in `src/schema/schema_runtime.cpp:713`. |
| Memory Pool | Yes | `BufferPool` in `include/keydrop/core/buffer_pool.hpp:17` and `PayloadPool` in `include/keydrop/schema/payload_pool.hpp:56`; runtime setters appear in `src/schema/schema_runtime.cpp:764` and `src/schema/schema_runtime.cpp:774`. |
| Dictionary Optimization | Yes | `AdaptiveDictionary` is declared in `include/keydrop/schema/adaptive_dictionary.hpp:34`; string encode/decode uses dictionary lookup/reference logic in `src/schema/schema_runtime.cpp:67` and `src/schema/schema_runtime.cpp:150`. |
| Adaptive Runtime | Yes | `StreamOptimizer` tracks sample counts and switches behavior after `aggressive_after_samples` in `src/schema/stream_optimizer.cpp:32`; dictionary entries adapt through create/lookup/evict logic in `src/schema/adaptive_dictionary.cpp`. |
| Compression | No | No general-purpose compression API, codec, or dependency was found in `include/` or `src/`. The only repository matches are README planned text and a test comment; implemented size reduction is represented separately as runtime optimization and dictionary optimization. |
| Streaming Support | Yes | `SchemaRuntime::send_stream`, `flush_stream`, and `receive_stream` are declared in `include/keydrop/schema/schema_runtime.hpp:80` and implemented in `src/schema/schema_runtime.cpp:649`, `src/schema/schema_runtime.cpp:670`, and `src/schema/schema_runtime.cpp:683`. |
| Zero Copy Access | No | `BufferView` is present in `include/keydrop/core/buffer.hpp`, but the schema decode path copies decoded strings/bytes into `FieldValue` objects via `PacketReader`; no typed zero-copy field accessor was found. |
| Configuration System | Yes | Config structs and setters exist for runtime optimization, dictionary optimization, stream optimization, memory pools, and transports: `RuntimeOptimizerConfig`, `AdaptiveDictionaryConfig`, `StreamOptimizerConfig`, `BufferPoolConfig`, `PayloadPoolConfig`, and `TransportConfig`. |
| Benchmark Framework | Yes | Benchmark APIs are in `include/keydrop/benchmark/format_benchmark.hpp`; `run_format_benchmarks` is implemented in `src/benchmark/format_benchmark.cpp:432`; `benchmark_runner` target is defined in `CMakeLists.txt:489`. |
| Testing Framework | Yes | CMake enables testing at `CMakeLists.txt:107` and defines many `add_test` targets; tests are present under `tests/`, including schema, runtime, optimizer, transport, reliability, and benchmark tests. |

## External Documentation Basis for Non-Keydrop Rows

- JSON: JSON is a text, language-independent data-interchange format that is easy for humans to read and write, per https://www.json.org/json-en.html. JSON Schema documents validation/schema support at https://json-schema.org/.
- Protocol Buffers: Official overview states Protocol Buffers are language-neutral, platform-neutral, structured-data serialization with `.proto` definitions and generated code: https://protobuf.dev/overview/.
- MessagePack: Official site describes it as an efficient binary serialization format similar to JSON and supported across many languages: https://msgpack.org/. The specification states MessagePack is object serialization with byte-array formats: https://github.com/msgpack/msgpack/blob/master/spec.md.
- FlatBuffers: Official docs describe it as an efficient cross-platform serialization library with schema support, open source licensing, and direct access to serialized data without parsing/unpacking: https://flatbuffers.dev/.
- Cap'n Proto: Official introduction describes binary data interchange with no encode/decode step, platform-independent byte layout, random access, and schema language: https://capnproto.org/ and https://capnproto.org/language.html.
