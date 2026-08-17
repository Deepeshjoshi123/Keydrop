# Keydrop Technical Research Draft

This document is the source research draft for a future IEEE-style systems paper on Keydrop. It is intentionally grounded in the repository implementation. Performance claims must be filled only after running the reproducible experiments in `research/benchmark/`.

## 1. Problem Statement

Telemetry systems frequently exchange small structured messages at high rates. General-purpose serialization formats can carry these messages, but the implementation in this repository targets a narrower problem: schema-aware binary telemetry packets, runtime packet minimization, stream-oriented reuse, reliability checks, and transport delivery in one C++ runtime.

The research problem is to evaluate whether a telemetry-specific runtime can reduce packet overhead and improve repeated-message handling while preserving validation, decoding correctness, and cross-platform transport behavior.

## 2. Motivation

The repository README describes Keydrop as an "Adaptive Real-Time Telemetry Transport Engine" focused on adaptive runtime optimization, schema-aware packet minimization, low-latency distributed communication, and reliability-aware packet processing. The implementation supports this direction through:

- Binary encoding and decoding in `src/core/encoder.cpp` and `src/core/packet_reader.cpp`.
- Schema registration, validation, mapping, and runtime send/receive in `src/schema/schema_runtime.cpp`.
- Runtime zero-value omission in `src/schema/runtime_optimizer.cpp`.
- Adaptive string dictionary support in `src/schema/adaptive_dictionary.cpp`.
- Stream packet reuse and batching in `src/schema/stream_optimizer.cpp`.
- Corruption detection and packet recovery in `src/reliability/`.
- TCP and WebSocket-style transport adapters in `src/transport/`.

## 3. Research Objectives

1. Characterize Keydrop's architecture and packet lifecycle from schema definition to transport.
2. Measure packet size, encode latency, decode latency, throughput, allocation count, and allocation bytes against baseline formats implemented in the repository.
3. Evaluate optimization features separately: zero-value omission, adaptive dictionary reuse, stream batching, packet reuse, and recovery behavior.
4. Document correctness, portability, memory behavior, and limitations using unit tests and reproducible benchmark scripts.

## 4. Existing Systems

The comparison set for the final paper should include JSON, Protocol Buffers, MessagePack, FlatBuffers, and Cap'n Proto. The current repository benchmark implementation includes in-repo encoders for JSON, protobuf-like tagged binary data, and MessagePack-like map data in `src/benchmark/format_benchmark.cpp`. It does not link against official external libraries for these formats.

Therefore, repository-native results should be labeled as "in-repo baseline encodings." A future extended study may add official libraries for Protocol Buffers, MessagePack, FlatBuffers, and Cap'n Proto.

## 5. Comparison With Existing Systems

| System | Serialization model | Schema management | Runtime behavior | Zero-copy support | Transport awareness | Repository benchmark status |
| --- | --- | --- | --- | --- | --- | --- |
| JSON | Text object encoding | Usually external or implicit | Human-readable, larger key overhead for telemetry | No inherent zero-copy model | None inherent | Implemented as string construction and parsing |
| Protocol Buffers | Binary tagged fields | `.proto` schema and generated code in normal use | Compact binary fields and backward-compatible field tags | Limited, depends on implementation | None inherent | Implemented as simplified tagged binary baseline |
| MessagePack | Binary object/value encoding | Usually schema-less | Compact binary representation of maps/arrays/scalars | Depends on implementation | None inherent | Implemented as simplified map baseline |
| FlatBuffers | Binary schema-based layout | IDL and generated accessors | Designed for direct reads from serialized buffers | Yes, by design | None inherent | Not implemented in current repo |
| Cap'n Proto | Binary schema-based message layout | Schema and generated code | Designed for direct use without parse/unpack step | Yes, by design | RPC support exists in ecosystem | Not implemented in current repo |
| Keydrop | Binary schema-aware telemetry packet | Runtime `SchemaDef` registration | Runtime validation, optimization, dictionary reuse, stream batching | Uses non-owning `BufferView`, but full zero-copy end-to-end is not implemented | TCP/WebSocket adapter abstraction and scheduler | Implemented |

Research gap: Keydrop attempts to combine telemetry-specific schema encoding with runtime optimization and transport/reliability utilities in one small C++ runtime. This differs from serialization-only libraries. Any claim of superiority must be measured under the methodology in this workspace.

## 6. System Architecture

Keydrop is organized as a static C++17 library built by CMake. Its major modules are:

- `core`: byte buffer, buffer views, buffer pool, endian conversion, packet builder, packet reader, and primitive encoder.
- `schema`: schema definitions, registry, validator, field mapper, packet layout builder, JSON conversion types, schema runtime, runtime optimizer, adaptive dictionary, payload pool, and stream optimizer.
- `reliability`: corruption detection and packet synchronizer.
- `transport`: transport interface, TCP adapter, WebSocket adapter wrapper, transport configuration factory, and scheduler.
- `benchmark`: timing, allocation accounting, format benchmark encoders, and report formatting.
- `platform`: cross-platform socket subsystem helpers.

The central integration point is `SchemaRuntime`. It owns a `SchemaRegistry`, optimizer configuration, mutable adaptive dictionary, stream optimizer, buffer pool, and payload pool.

## 7. Core Runtime

The core runtime writes primitive values into a `Buffer` through `PacketBuilder` and `Encoder`. `Encoder` converts multi-byte integers to little-endian representation and writes floating-point values by copying IEEE-style bit patterns into integer storage before encoding. Strings and bytes are length-prefixed with `u16`.

`PacketReader` performs the inverse operation and throws `std::out_of_range` when reads exceed packet boundaries. `SchemaRuntime` catches these exceptions and converts them into structured runtime errors.

## 8. Schema Runtime

The schema model is defined by `SchemaDef`, `FieldDef`, `FieldType`, and `FieldConstraints`. Supported field types are `u8`, `u16`, `u32`, `i8`, `i16`, `i32`, `f32`, `f64`, `string`, and `bytes`.

The runtime flow is:

1. Validate and register a schema.
2. Build and cache a `PacketLayout` in the registry.
3. Map a named payload to ordered schema fields.
4. Validate payload types and constraints.
5. Encode the two-byte message id followed by ordered field payloads.
6. Optionally optimize the packet.
7. Decode by message id lookup, deoptimization, corruption check, field decoding, validation, and ordered-to-named mapping.

## 9. Runtime Optimization

`RuntimeOptimizer` currently implements optional zero-value omission for fixed-size fields. Optimized packets keep the original two-byte message id, add marker `0xFD`, store a bitmap size, a bitmap identifying omitted zero fields, and then the remaining body bytes. Optimization is applied only if the candidate packet is smaller than the original.

Variable-length `string` and `bytes` fields are copied through during this optimization; they are not zero-omitted.

## 10. Reliability Layer

`CorruptionDetector` validates minimum size, optional header marker, optional length prefix, checksum callback, CRC32, and Keydrop packet structure. `check_keydrop_packet` uses packet layout information to verify fixed and variable field boundaries.

`PacketSynchronizer` attempts to recover packets from a byte stream using registered schema layouts. The schema runtime exposes `receive_recovered_stream` for recovering all packets and reporting skipped bytes.

## 11. Transport Layer

The transport interface defines connect, listen, close, send, and receive operations. `TcpAdapter` uses native sockets and sends packets with a four-byte little-endian length prefix followed by packet bytes. `WebSocketAdapter` wraps `TcpAdapter` and validates a path, but the current implementation should be verified before describing it as a full RFC WebSocket protocol implementation.

`TransportScheduler` queues `Buffer` packets and flushes them through any `Transport`.

## 12. Memory Management

`Buffer` owns a `std::vector<byte>`. `BufferView` provides non-owning views and slices. `BufferPool` reuses `Buffer` instances through RAII leases, and `PayloadPool` reuses ordered and named payload containers. The in-repo allocation tracker uses thread-local counters and global `new`/`delete` overrides while the encode timing window is active. It records gross heap allocation activity in that process window; it is not a measure of total process memory, peak resident memory, or retained allocations.

## 13. Cross Platform Design

The repository uses CMake and C++17. Platform socket differences are isolated in `src/platform/socket.cpp` and `src/transport/tcp_adapter.cpp`. Windows links `ws2_32`; POSIX builds use standard socket headers and `close`. README build instructions cover Linux, Visual Studio 2022, and MinGW-w64.

## 14. Experimental Methodology

Experiments should use manifest-backed Release studies, fixed CPU/power settings where possible, warm-up runs, repeated trials, raw CSV capture, processed summary generation, and graph scripts. See `research/methodology/methodology.md`.

## 15. Benchmark Design

Benchmark dimensions:

- Packet size in bytes.
- Encoding latency in nanoseconds.
- Decoding latency in nanoseconds.
- Throughput in operations per second.
- Logical allocation count.
- Logical allocated bytes.
- Bandwidth efficiency and payload reduction relative to baselines.
- Stream optimizer emitted packets, emitted bytes, decoded messages, throughput, and latency.

Workloads should include temperature, humidity, GPS, vehicle telemetry, IoT monitoring, mixed telemetry, large payloads, high-frequency streams, low-frequency streams, constant streams, variable streams, and string-heavy payloads.

## 16. Experimental Results

Placeholder. Do not add numerical claims here until a manifest-backed study in `research/benchmark/studies/` has generated and processed its raw CSV files.

Expected generated artifacts:

- `research/benchmark/studies/<study-id>/manifest.json`
- `research/benchmark/studies/<study-id>/raw/format_trials.csv`
- `research/benchmark/studies/<study-id>/raw/stream_trials.csv`
- `research/benchmark/studies/<study-id>/processed/*.csv`
- `research/benchmark/studies/<study-id>/graphs/*.{png,svg,pdf}`

## 17. Discussion

The implementation is strongest where telemetry messages are small, schema-known, and repeated over time. Optimization and batching are especially relevant for stable streams. The main research question is not whether binary encoding is smaller than text in general, but whether Keydrop's combined runtime choices are beneficial under realistic telemetry workloads.

## 18. Limitations

- Current format benchmarks use simplified in-repo baselines, not official Protocol Buffers or MessagePack libraries.
- FlatBuffers and Cap'n Proto are not implemented in the repository benchmark.
- Allocation tracking measures gross heap-allocation activity only during the encode timing window, not total process memory.
- The WebSocket adapter should not be described as a complete WebSocket protocol implementation without additional evidence.
- No benchmark numbers are present until experiments are run.
- Security properties such as authentication, encryption, replay protection, and adversarial fuzzing are outside the current implementation.

## 19. Future Work

- Add official library baselines for Protocol Buffers, MessagePack, FlatBuffers, and Cap'n Proto.
- Add platform-comparable benchmark automation for Linux and Windows.
- Add global allocation instrumentation or platform profilers.
- Extend stream optimization with explicit delta packet formats if implemented.
- Add fuzz testing for packet reader, schema runtime, and synchronizer.
- Add transport benchmarks over loopback and real networks.

## 20. Conclusion

Keydrop is an implemented C++17 telemetry runtime with schema-aware binary packets, runtime optimization, stream optimization, memory pooling, reliability checks, and transport abstractions. The repository provides enough implementation surface to support a systems paper, provided the final paper clearly separates verified implementation facts from benchmark results that still need to be generated.

## Traceability Map

| Topic | Repository evidence |
| --- | --- |
| Build and tests | `CMakeLists.txt`, `README.md` |
| Core buffer and encoding | `include/keydrop/core/`, `src/core/` |
| Schema runtime | `include/keydrop/schema/schema_runtime.hpp`, `src/schema/schema_runtime.cpp` |
| Runtime optimization | `src/schema/runtime_optimizer.cpp` |
| Dictionary reuse | `include/keydrop/schema/adaptive_dictionary.hpp`, `src/schema/adaptive_dictionary.cpp` |
| Stream optimization | `src/schema/stream_optimizer.cpp`, `benchmarks/stream_optimizer_benchmark.cpp` |
| Reliability | `include/keydrop/reliability/`, `src/reliability/` |
| Transport | `include/keydrop/transport/`, `src/transport/` |
| Benchmarks | `include/keydrop/benchmark/`, `src/benchmark/`, `benchmarks/` |
