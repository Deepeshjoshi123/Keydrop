# Keydrop — Integration Guide

This directory shows how to integrate Keydrop into a real C++ application.
Each example is a self-contained `.cpp` file you can build, run, and read.

## Architecture (30 seconds)

```
  Your Application
       │
       ├─ define SchemaDef (field names, types, constraints)
       ├─ register with SchemaRuntime
       │
       ├─ SEND PATH                          RECEIVE PATH
       │   payload (map or vector)               raw bytes from wire
       │        │                                     │
       │        ▼                                     ▼
       │   SchemaRuntime::send()              SchemaRuntime::receive()
       │        │                                     │
       │        ▼                                     ▼
       │   validate → map → encode            message_id lookup → decode → validate
       │        │                                     │
       │        ▼                                     ▼
       │   [optional] RuntimeOptimizer        named payload (map of fields)
       │   [optional] AdaptiveDictionary
       │   [optional] StreamOptimizer
       │        │
       │        ▼
       │   binary Buffer (17-100+ bytes)
       │        │
       │        ▼
       │   Transport::send() ─── TCP ───▶ Transport::receive()
```

Keydrop sits between your data structures and the wire. You feed it typed payloads; it returns compact binary packets.

## Directory Layout

```
integration/
├── README.md                       ← you are here
├── configs/
│   ├── telemetry_schema.json       ← example: 6-field sensor schema
│   ├── runtime_config.json         ← optimizer, dictionary, pool settings
│   └── transport_config.json       ← TCP endpoint config
├── scripts/
│   ├── generate_schema.py          ← generate custom schema JSON from CLI
│   ├── generate_telemetry_data.py  ← generate random sensor CSV/JSON
│   ├── run_benchmark.sh            ← build + run the format benchmark
│   └── demo_tcp.sh                 ← one-command TCP server+client demo
├── 01-send-receive.cpp             ← minimal: schema → send → receive
├── 02-with-dictionary.cpp          ← adaptive string deduplication
├── 03-stream-batching.cpp          ← packet reuse + batch envelopes
├── 04-tcp-server.cpp               ← TCP transport: server side
├── 04-tcp-client.cpp               ← TCP transport: client side
├── 05-corruption-recovery.cpp      ← recover packets from noisy streams
├── CMakeLists.txt                  ← builds all examples
└── run_all.sh                      ← build + run everything in sequence
```

## Quick Start

```bash
# From the repository root:
./integration/run_all.sh

# Or build and run a single example:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target 01-send-receive
./build/integration/01-send-receive
```

## Example Walkthrough

### 01 — Send & Receive

The simplest possible integration. Defines a 3-field schema (`temperature`, `humidity`, `device_id`), encodes a payload, decodes it back, and verifies the round-trip.

**Key APIs:** `SchemaDef`, `SchemaRuntime::register_schema()`, `SchemaRuntime::send()`, `SchemaRuntime::receive()`

### 02 — Adaptive Dictionary

Enables the runtime dictionary. The first time a string appears it's sent in full. Subsequent occurrences are replaced on the wire with a 2-byte ID — the dictionary learns automatically.

**Key APIs:** `AdaptiveDictionaryConfig`, `SchemaRuntime::set_dictionary_config()`

**Expected output:** second packet is smaller than the first for the same string value.

### 03 — Stream Batching

Enables the stream optimizer. Identical payloads are suppressed entirely (packet reuse). Low-change payloads are queued into batch envelopes and emitted together via `flush_stream()`.

**Key APIs:** `StreamOptimizerConfig`, `SchemaRuntime::send_stream()`, `SchemaRuntime::flush_stream()`, `SchemaRuntime::receive_stream()`

**Expected output:** identical message produces no bytes. Several low-change messages queue up and are flushed as one batch packet.

### 04 — TCP Transport

A two-process demo. The server listens on `127.0.0.1:9876`, the client connects and sends 3 encoded Keydrop packets. The server receives and decodes them.

**Key APIs:** `TcpAdapter`, `TransportEndpoint`, `Transport::send()`, `Transport::receive()`

**Run manually:**
```bash
# Terminal 1:
./build/integration/04-tcp-server

# Terminal 2:
./build/integration/04-tcp-client
```

### 05 — Corruption Recovery

Builds a deliberately corrupted byte stream (noise bytes + valid packet + noise + valid packet). The `receive_recovered_stream()` API skips garbage and recovers both valid packets, reporting how many bytes were skipped.

**Key APIs:** `SchemaRuntime::receive_recovered_stream()`, `PacketSynchronizer`

**Expected output:** 2 valid packets recovered, 8 bytes skipped (5 prefix + 3 mid-stream).

## Configuration Files

The `configs/` directory contains JSON files that define what your application needs:

- **`telemetry_schema.json`** — field definitions (name, type, constraints). This is what you'd load at startup and feed to `SchemaRuntime::register_schema()`.
- **`runtime_config.json`** — toggles for the optimizer, dictionary, stream optimizer, and memory pools. Tune these for your workload.
- **`transport_config.json`** — server/client host:port pairs. Your deployment config.

Use `send_json()` / `receive_json()` if you want to drive Keydrop directly from these JSON configs at runtime.

## Helper Scripts

```bash
# Generate a custom schema from CLI arguments:
python3 scripts/generate_schema.py \
    --name BatteryTelemetry --id 200 \
    --field voltage:u16 --field current:i16 --field cell_id:string:32 \
    > my_schema.json

# Generate 500 rows of random sensor data:
python3 scripts/generate_telemetry_data.py --format csv --rows 500 > test_data.csv

# Run the format benchmark:
./scripts/run_benchmark.sh 100000

# Run the TCP demo end-to-end:
./scripts/demo_tcp.sh
```

## Integrating Into Your Own Project

1. **Add Keydrop as a CMake subdirectory** (or link against the static library).
2. **Define your schemas** — one `SchemaDef` per message type.
3. **Create one `SchemaRuntime`** — register all schemas at startup.
4. **Configure the runtime** — dictionary, optimizer, pools (or leave defaults).
5. **Call `send()` / `receive()`** in your data pipeline.
6. **Optionally** wire up `TcpAdapter` for network transport.

That's it. Keydrop has zero external dependencies — just link and go.

## Further Reading

- [Full API headers](../include/keydrop/)
- [Benchmark suite](../benchmarks/)
- [Research paper draft](../research/technical_draft.md)
- [Project README](../README.md)
