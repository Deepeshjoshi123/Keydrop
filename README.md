# Keydrop

**Adaptive Real-Time Telemetry Transport Engine** — a zero-dependency C++17
library that turns your JSON telemetry into tiny binary packets.

Describe your data once in YAML, keep sending normal JSON, and Keydrop
handles validation, compact binary encoding, string dictionaries, delta
compression, and transport (TCP / WebSocket / UDP) for you.

```
JSON in  ──►  YAML schema check  ──►  compact binary packet  ──►  JSON out
```

---

## 60-second start (no C++ needed)

```bash
# 1. Build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# 2. Validate your schema
./build/bin/keydrop_cli validate integration/configs/beginner_sensor.yaml

# 3. See what your schema looks like
./build/bin/keydrop_cli inspect integration/configs/beginner_sensor.yaml SensorReading

# 4. Generate an example JSON payload for any schema
./build/bin/keydrop_cli example integration/configs/beginner_sensor.yaml SensorReading

# 5. Encode a JSON payload into a binary packet
./build/bin/keydrop_cli encode integration/configs/beginner_sensor.yaml \
    SensorReading integration/configs/beginner_sensor.json

# 6. Decode it back to JSON
./build/bin/keydrop_cli decode integration/configs/beginner_sensor.yaml \
    <the-hex-from-step-5>
```

The example configuration is 11 lines:

```yaml
keydrop: 1

schemas:
  SensorReading:
    id: 42
    version: 1
    profile: telemetry-balanced
    fields:
      - key: temperature
        type: uint16
      - key: humidity
        type: uint16
      - key: device_id
        type: string
        max_length: 64
```

The `profile:` line picks a named configuration —
`telemetry-low-latency`, `telemetry-balanced`, `telemetry-bandwidth`, or
`telemetry-lossless-archive` — and Keydrop can adapt its optimizations to
your stream automatically. Your explicit settings always win.

## C++ quick start

```cpp
#include <keydrop/schema/schema_runtime.hpp>
using namespace keydrop;

// 1. Describe your data once.
SchemaRuntime runtime;
const SchemaDef sensor {
    "SensorReading", 42,
    {
        FieldDef{"temperature", FieldType::u16, 0, {}},
        FieldDef{"humidity",    FieldType::u16, 1, {}},
        FieldDef{"device_id",   FieldType::string, 2, FieldConstraints{true, 64}},
    }
};
runtime.register_schema(sensor);

// 2. Send — same call every time.
NamedPayload payload;
payload["temperature"] = FieldValue::from_u16(23);
payload["humidity"]    = FieldValue::from_u16(71);
payload["device_id"]   = FieldValue::from_string("sensor-01");

Buffer packet;
runtime.send("SensorReading", payload, packet);      // 17 bytes

// 3. Receive — back to named fields.
std::string schema_name;
NamedPayload decoded;
runtime.receive(packet, schema_name, decoded);
// decoded["temperature"].as_u16 == 23
```

**Send JSON directly** if you prefer:

```cpp
runtime.send_json("SensorReading", json_object, packet);
runtime.receive_json(packet, schema_name, json_object);
```

**Go faster** with the zero-copy fast path (no allocations in steady state):

```cpp
FieldValue values[3] = {FieldValue::from_u16(23), FieldValue::from_u16(71),
                        FieldValue::from_string("sensor-01")};
runtime.fast_encode("SensorReading", values, 3, packet);   // reuses packet's memory

FastDecodedField fields[3];
usize count = 0;
runtime.fast_decode(packet, schema_name, fields, 3, count); // fields[2].view = zero-copy string
```

**Ship it over the network**:

```cpp
#include <keydrop/transport/tcp_adapter.hpp>

TcpAdapter tcp;
tcp.connect({"127.0.0.1", 9876, ""});
tcp.send(packet);
TransportReceiveResult received = tcp.receive();  // received.packet
```

`TcpAdapter`, `WebSocketAdapter` (real RFC 6455), and `UdpAdapter` all
speak the same `Transport` interface. TCP has timeouts, reconnect, and
backpressure built in; WebSocket handles the handshake, masking, and
control frames; UDP detects loss and reordering.

**Make streams tiny** with the stateful mode:

```cpp
StreamOptimizerConfig cfg;
cfg.enabled = true;
cfg.enable_delta_packets = true;   // change-only fields + signed deltas
runtime.set_stream_optimizer_config(cfg);

bool has_packet = false;
runtime.send_stream("SensorReading", payload, packet, has_packet);
```

Identical values are never re-sent; slowly changing values travel as tiny
deltas; a full keyframe re-synchronizes every 100 records; loss is
detected and rejected, never silently misdecoded.

## The workloads we tested

Every benchmark runs real workloads, so you know exactly what each number
means. A "record" is one telemetry message (for example one sensor reading).

| Workload | What it looks like in practice | What Keydrop measured |
|---|---|---|
| **W1 — fixed record** | One small message, same shape every time: temperature, humidity, device ID (17-byte packet) | 17 B vs JSON 56 B (−70%), Protobuf 15 B, MessagePack 13 B |
| **W2 — sparse fields** | Many fields are often zero or unset (status flags, optional readings) | Zero-values and unchanged fields are skipped via bitmaps instead of being sent |
| **W3 — repeated strings** | The same device IDs, status labels, and units appear over and over | String dictionary: 56.8% smaller steady-state messages (37 B → ~16 B) |
| **W4 — periodic timestamps** | A timestamp that increases on a fixed schedule (every second, every poll) | Delta stream: 22 B → 10.4 B per record (−52.5%), loss never misdecodes |
| **W5 — slowly changing sensors** | Temperature, humidity, GPS drifting a little between reads | Signed deltas send only the small difference, not the whole value |
| **W6 — string-heavy records** | Messages dominated by IDs, states, and labels | Combined dictionary + deltas: 79.7% smaller than stateless |
| **W7 — batched streams** | Sending many records together | Batching reduces the number of sends; the byte win comes from the dictionary and deltas |
| **W8 — end-to-end transport** | Full trip over TCP / WebSocket / UDP | Byte-identical round-trips; wire accounting reports payload, Keydrop framing, and transport framing separately |
| **W9 — loss and corruption** | Dropped packets, corrupted bytes, reconnects | Dropped deltas are rejected (never misdecoded) and recover at the next keyframe; optional CRC32 envelopes detect corruption; decode paths are fuzz-tested |

The exact commands, raw data, and figures for these runs live in
`research/benchmark/` — each study records the commit, machine, and
environment it was measured on.

## What Keydrop gives you

| Area | Feature |
|---|---|
| **Configuration** | YAML schemas, actionable validation errors, schema IDs and versions, fingerprints, compatibility checks between peers |
| **Wire format** | Compact little-endian binary: message ID + fixed-width fields + length-prefixed strings/bytes |
| **Bandwidth** | String dictionary, change-only bitmaps, signed delta coding, batching, optional CRC32 envelopes |
| **Adaptive** | Named profiles + automatic per-stream optimization; user settings always win |
| **Speed** | Zero-allocation steady-state encode/decode, zero-copy string views, buffer/payload pools |
| **Reliability** | Corruption detection, safe resynchronization, sequence numbers, keyframes, fuzz-tested decode paths |
| **Transport** | TCP (timeouts / reconnect / backpressure), WebSocket (RFC 6455), UDP with loss detection |
| **Tooling** | `keydrop_cli`: validate, inspect, example, encode, decode, compatibility |

## Build and test

Needs CMake ≥ 3.20 and a C++17 compiler. Linux, Windows (MSVC / MinGW-w64).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure       # 35 tests
```

Optional (enables the official-library comparison benchmark):

```bash
sudo apt-get install protobuf-compiler libprotobuf-dev nlohmann-json3-dev libmsgpack-dev
```

## Benchmarks

```bash
./build/bin/benchmark_runner 100000 build/benchmark_report.txt   # format comparison
./build/bin/fastpath_benchmark 200000                            # fast path
./build/bin/stateful_bandwidth_benchmark                         # dictionary / delta / batch
./build/bin/adaptive_profiles_benchmark                          # adaptive decisions
./build/bin/transport_bytes_benchmark                            # wire byte accounting
./build/bin/official_baselines_benchmark 100000                  # vs JSON / Protobuf / MessagePack
```

Reproducible studies (commit + environment + raw CSV + processed data +
figures together):

```bash
python3 research/benchmark/scripts/run_study.py \
  --study-id my-study-$(date +%Y%m%d) \
  --build-dir build/research --iterations 100000 --trials 30
```

## Project layout

```
include/keydrop/   — public API (schema, transport, reliability, benchmark)
src/               — implementation
tools/             — keydrop_cli (validate / inspect / example / encode / decode / compatibility)
integration/       — copy-pasteable C++ examples + beginner YAML/JSON configs
benchmarks/        — the benchmarks above
tests/             — 35-test suite (including fuzz and wire-level transport tests)
docs/              — detailed guides per feature area
research/          — reproducible study runner, raw/processed data, figures
```

## License

MIT
