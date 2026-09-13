# Keydrop Phase-Wise Optimization and Publication Research Plan

## Purpose

This document defines an evidence-first engineering and research plan for Keydrop. Its goal is to make Keydrop easy to use for telemetry applications while rigorously evaluating bandwidth reduction, runtime cost, reliability, and interoperability.

The central hypothesis is:

> For schema-known, repetitive telemetry streams, Keydrop's configurable stateful encoding can reduce transmitted bytes compared with stateless representations while preserving correct reconstruction, bounded latency, and a beginner-friendly JSON-facing API.

This plan is not a plan to manufacture a favourable comparison. Every raw result must be retained and the final paper must report the final measured results, including unmet targets and negative results.

## Research Integrity Rules

- Freeze the source commit and benchmark configuration before each reported study.
- Never edit raw benchmark output by hand.
- Keep raw data, processed data, plotting code, commands, environment capture, and figures together.
- Define workloads and pass/fail gates before implementation begins.
- Separate cold-start, steady-state, and amortized stream measurements.
- Do not compare Keydrop's stateful stream mode with a stateless baseline without labeling the distinction.
- Use official Protocol Buffers C++ and MessagePack C++ implementations for final external-library claims.
- Do not call allocation instrumentation total process memory unless it actually measures total process memory.
- Preserve existing verified results; new work belongs in a separately versioned benchmark study.

## Core Metrics and Formulae

### Packet-size reduction

For a Keydrop packet of size `B_keydrop` and a baseline packet of size `B_baseline`:

```text
R (%) = 100 x (B_baseline - B_keydrop) / B_baseline
```

### Improvement from an optimization phase

```text
I_phase (%) = 100 x (B_previous - B_new) / B_previous
```

### Amortized stream bytes

```text
B_stream_avg = (B_setup + sum(B_record_i)) / M
```

Where `B_setup` includes schema, dictionary, or session setup bytes, and `M` is the number of records. Cold-start bytes and steady-state bytes must also be reported independently.

### Useful-data efficiency

```text
E = useful_payload_bytes / total_on_wire_bytes
```

`total_on_wire_bytes` must distinguish application payload, Keydrop framing, and transport framing.

### Throughput

```text
T = operations / (encode_time + decode_time)
```

If a benchmark uses a different definition, use the code's exact definition consistently in the paper and figures.

## Common Benchmark Protocol

Every phase that changes a measured path must run the same protocol:

- Release build from a recorded Git commit.
- Exact CPU model, RAM, operating system, compiler, compiler version, CMake version, generator, and compiler flags recorded.
- Warm-up run discarded.
- At least 30 independent trials for publication figures.
- Randomized/interleaved benchmark order where supported.
- Raw CSV output retained.
- Report mean, median, standard deviation, p95, p99, min, max, and sample count where applicable.
- Report packet bytes, encode latency, decode latency, combined throughput, allocation count, and allocated bytes using precise instrumentation terminology.
- Require semantic JSON round-trip correctness for every valid test payload.

## Workload Matrix

| ID | Workload | Primary question |
|---|---|---|
| W1 | Fixed three-field record | What is the raw stateless codec cost? |
| W2 | Sparse optional telemetry | Does a presence bitmap save bytes? |
| W3 | Repeated device IDs/status strings | Does dictionary encoding save steady-state bytes? |
| W4 | Periodic timestamps and counters | Do delta encodings save bytes safely? |
| W5 | Slowly changing GPS/sensor values | Do deltas outperform fixed-width fields? |
| W6 | Variable-size/string-heavy records | Where are the limits of the design? |
| W7 | Batched stream | What is the framing/latency trade-off? |
| W8 | End-to-end local transport | What is the runtime cost beyond raw serialization? |
| W9 | Loss/corruption/reconnect simulation | Can stateful decoding recover safely? |

## Phase 0 - Trusted Baseline and Reproducibility

### Research question

What does the current implementation cost for each workload under a fully documented environment?

### Tasks

- Reconcile the existing raw reports, processed CSV, figures, and manuscript values before using them as a publication dataset.
- Add a final-run manifest containing commit hash, dirty state, command, iteration count, trial count, environment, and output paths.
- Make every published figure regenerate from the selected processed data.
- Add official external-library benchmark targets separately from in-repository development baselines.
- Build and run the complete test suite in a clean Release build and archive the result.

### Gates

| Gate | Target |
|---|---:|
| Raw-data to figure reproducibility | 100% |
| Environment fields completed | 100% |
| JSON/binary round-trip correctness | 100% |
| Schema mismatch safely rejected | 100% |
| Untraceable published value | 0 |
| Release test failures | 0 |

No optimization phase should begin until this phase is complete.

## Phase 1 - Beginner-First YAML Configuration and JSON API

### Research question

Can a beginner configure and use Keydrop without manually handling binary packets or field identifiers?

### User model

Users write YAML configuration and send normal JSON locally. Keydrop validates JSON against the configuration, maps names to a shared schema, sends only compact binary values, and reconstructs named JSON at the receiver.

```text
JSON input
  -> YAML schema validation
  -> field-name to schema-field mapping
  -> compact binary packet
  -> receiver schema lookup
  -> JSON output
```

### Example configuration

```yaml
keydrop: 1

schemas:
  vehicle.telemetry:
    id: 101
    version: 1
    profile: telemetry-balanced

    fields:
      - key: timestamp
        type: timestamp_ms
        delta: true

      - key: device_id
        type: string
        dictionary: true

      - key: temperature_c
        type: decimal
        scale: 100

      - key: humidity_pct
        type: uint8
        range: [0, 100]
```

### Required product capabilities

- YAML parser and schema validator.
- Stable schema ID and version.
- Automatic stable field-ID generation with an advanced explicit-ID override.
- Schema fingerprint negotiation before stateful stream data.
- Generated example JSON and generated configuration documentation.
- Commands such as `validate`, `inspect`, `init --from-json`, `encode`, `decode`, and `compatibility`.
- Errors that name the field, expected type/range, actual value, and corrective action.
- Safe rejection of unknown schema IDs and versions.

### Gates

| Gate | Target |
|---|---:|
| JSON -> binary -> JSON semantic correctness | 100% |
| Invalid configuration safely rejected | 100% |
| Invalid payload safely rejected | 100% |
| Schema-version mismatch silently decoded | 0 |
| Existing stateless packet-size regression | 0% |
| Beginner first working configuration | <= 10 minutes |

## Phase 2 - Stateless Schema-Aware Fast Path

### Research question

Can Keydrop maintain compact packets while lowering generic-runtime overhead?

### Architecture work

- Precompute and cache field layout at schema registration time.
- Use schema-specialized codec functions or generated codecs for stable high-rate schemas.
- Add `encode_into(Buffer&)` so callers can reuse capacity.
- Reserve output capacity from the schema's maximum/expected packet size.
- Reuse output buffers through a bounded buffer pool.
- Avoid generic maps, temporary `FieldValue` objects, and string copies in the typed fast path.
- Decode borrowed strings/bytes as validated `BufferView` values only when ownership/lifetime is explicit and safe.
- Retain the existing general, validation-heavy path for dynamic inputs.

### Targets versus the current Keydrop fixed-record baseline

| Metric | Engineering target |
|---|---:|
| Packet size | No regression; <= current baseline |
| Encode latency | 20-35% lower |
| Decode latency | 20-40% lower |
| Throughput | 25-60% higher |
| Allocations per operation | 70-90% lower |
| Allocated bytes per operation | 50-80% lower |
| Round-trip failures | 0 |

These are engineering targets only. They are not guaranteed results and must not be presented as results before they are measured.

## Phase 3 - Stateful Bandwidth Optimization

### Research question

How much bandwidth can a schema-known telemetry stream save beyond a single stateless packet?

### 3A. Repeated-string dictionary

Use bounded, resettable dictionary references for repeated strings such as device IDs, status labels, locations, and units.

For the present illustrative 17-byte record (`2 B` message ID, `2 B` temperature, `2 B` humidity, and `11 B` length-prefixed `sensor_01` string), replacing the string with the current approximate `4 B` dictionary reference yields an estimated steady-state packet of `10 B`.

```text
100 x (17 - 10) / 17 = 41.18%
```

This is a design estimate, not a result. The experiment must report first-use, reset, and steady-state packets independently.

| Gate | Target |
|---|---:|
| Repeated-string steady-state reduction | 35-45% |
| Dictionary reset/recovery correctness | 100% |
| Unknown reference silently misdecoded | 0 |
| Dictionary memory limit respected | 100% |

### 3B. Presence bitmap and unchanged-field suppression

For sparse streams, transmit a bitmap identifying present/changed fields followed by only the selected field values.

| Unchanged fields | Expected stream-byte reduction target |
|---:|---:|
| 25% | 10-25% |
| 50% | 25-45% |
| 75% | 45-70% |
| 90% | 60-85% |

Disable this optimization when its bitmap/header overhead is not repaid by omitted values.

### 3C. Delta and delta-of-delta coding

Use configurable, lossless deltas for timestamps, counters, and slowly changing scaled values.

- Timestamp: encode the difference from the previous timestamp.
- Periodic timestamps: encode delta-of-delta.
- GPS: encode scaled integer-coordinate deltas.
- Counter: encode non-negative increments.
- Slowly changing sensor: encode signed delta when smaller than the ordinary representation.

| Workload | Expected field-byte reduction target |
|---|---:|
| Regular timestamps | 50-85% |
| Slowly changing signed sensor | 25-70% |
| GPS deltas after scaling | 30-75% |
| Random/unpredictable values | 0-10%; disable when ineffective |

Every delta configuration must define initial keyframe, reset, maximum delta range, periodic keyframe interval, loss recovery, and decoder resynchronization behavior.

### 3D. Batch framing

Batch only where latency budgets permit it.

| Batch size | Expected framing-byte reduction target |
|---:|---:|
| 10 records | 5-15% |
| 50 records | 10-25% |
| 100 records | 15-30% |

Measure queueing latency. A byte reduction is not accepted if it violates the configured maximum delay.

## Phase 4 - Adaptive Profiles and Explicit Configuration

### Research question

Can Keydrop choose a suitable lossless mode based on stream characteristics without penalizing unsuitable streams?

### Profiles

```yaml
profile: telemetry-low-latency
profile: telemetry-balanced
profile: telemetry-bandwidth
profile: telemetry-lossless-archive
```

### Predefined decision rules

```text
Enable dictionary when repeated-string ratio >= 60%
and dictionary hit rate >= 80%.

Enable change-only fields when unchanged-field ratio >= 40%.

Enable delta encoding when the encoded delta is smaller than the normal field representation.

Enable batching only when queueing delay remains <= configured maximum
and the minimum efficient batch size is reached.
```

### Gates

| Gate | Target |
|---|---:|
| Suitable stream: lower amortized bytes than fixed mode | 20-60% |
| Unsuitable stream: packet-size overhead | <= 5% |
| Incorrect automatic choice | < 5% of evaluated windows |
| Explicit user override honored | 100% |
| Lossless reconstruction | 100% |

Do not enable general compression by default for small telemetry messages. Evaluate compression only for batches large enough to repay its latency and framing cost.

## Phase 5 - Stateful Reliability and Recovery

### Research question

Can a stateful stream remain correct under loss, corruption, reconnection, and schema changes?

### Required mechanisms

- Schema fingerprint/session handshake.
- Dictionary reset/control messages.
- Sequence numbers.
- Periodic full keyframes.
- Configurable checksum/CRC where required.
- Safe packet resynchronization.
- Decoder memory limits.
- Unknown-schema/version rejection.
- Backward-compatible schema evolution policy.

### Gates

| Gate | Target |
|---|---:|
| Corrupted packet accepted as valid | 0 |
| Stateful desynchronization silently misdecodes payload | 0 |
| Recovery after configured keyframe interval | 100% |
| Fuzz-test crash or out-of-bounds read | 0 |
| Compatibility test failures | 0 |

## Phase 6 - Transport Scope

### Release order

1. Finish TCP: framing, partial reads/writes, reconnect, timeout, backpressure, and security strategy.
2. Implement real WebSocket only when browser connectivity is a user need.
3. Add an MQTT client adapter only for broker-based IoT use cases.
4. Add UDP or QUIC only for a defined low-latency/loss-tolerant use case.

Transport protocols do not automatically reduce Keydrop serialization bytes. Report application payload, Keydrop framing, and transport framing separately.

| Transport | Release gate |
|---|---|
| TCP | 100% integration tests; framing, reconnect, and backpressure verified |
| WebSocket | Standards-compliant handshake, framing, masking/control-frame interoperability tests |
| MQTT | Publish/subscribe, QoS, reconnect, and broker interoperability tests |
| UDP/QUIC | MTU, sequencing, loss, reordering, and recovery tests |

## Phase 7 - Official Comparative Study and Paper Update

### Research question

Where does Keydrop provide measurable value against official, properly configured alternatives?

### Comparison targets

- JSON library, named exactly.
- Official Protocol Buffers C++ generated code using a reasonable reuse/arena configuration.
- Official MessagePack C++ implementation.
- Keydrop stateless schema mode.
- Keydrop stateful stream mode.
- FlatBuffers only if it is fully integrated and benchmarked.

### Expected interpretation

| Workload | Scientifically honest expectation |
|---|---|
| Single fixed record | Keydrop may be competitive in bytes; Protobuf may win raw latency |
| JSON comparison | Keydrop should substantially reduce bytes |
| Repeated strings | Keydrop stateful mode should target 35-45% lower steady-state bytes than its own stateless baseline |
| Sparse fields | Keydrop should target 25-70% lower bytes depending on sparsity |
| Regular timestamp stream | Keydrop should target 25-60% lower amortized bytes |
| Random fields | Keydrop may provide little benefit; report this result |
| End-to-end telemetry stream | Demonstrate runtime trade-offs, not universal codec superiority |

### Publication gates

- Every figure and table regenerated from processed data.
- Every processed value traceable to raw output and commit hash.
- Exact environment table contains no placeholders.
- Workload definitions are explicit and reproducible.
- Official-library and in-repository baseline results are never conflated.
- Stateful and stateless comparisons are clearly labeled.
- Limitations cover platform scope, workload scope, network scope, memory instrumentation, and baseline scope.
- The conclusion does not claim universal superiority.

## Final Completion Checklist

### Correctness

- [ ] YAML configuration validates deterministically.
- [ ] JSON input round-trips semantically through binary encoding.
- [ ] Schema versioning and compatibility are safe.
- [ ] Dictionary reset and recovery are tested.
- [ ] Corruption and loss recovery are tested.
- [ ] Fuzz tests have no crashes or out-of-bounds reads.

### Performance

- [ ] Cold-start, steady-state, and amortized measurements are separate.
- [ ] Application, Keydrop, and transport bytes are separate.
- [ ] Allocation terminology matches the implemented measurement method.
- [ ] Official baselines are used for external performance claims.
- [ ] Raw data, processed data, and plots are reproducible.

### Usability

- [ ] Beginner YAML template exists.
- [ ] Example JSON input exists.
- [ ] Documentation is generated from schema configuration.
- [ ] Errors are understandable without deep C++ knowledge.
- [ ] CLI supports validate, inspect, encode, decode, and compatibility checks.
- [ ] Advanced optimization settings remain opt-in.

### IEEE Submission

- [ ] Each technical claim is supported by source code, benchmark data, or a citation.
- [ ] Every claim states its workload and scope.
- [ ] Hardware/software environment and command are recorded.
- [ ] Repository commit and dirty state are recorded.
- [ ] Final Release test log is retained.
- [ ] Limitations are explicit.

## Recommended Final Positioning

Keydrop should be presented as a beginner-configurable, schema-driven telemetry runtime that accepts JSON locally and sends compact binary streams internally. Its strongest evidence should focus on bandwidth reduction for repetitive, sparse, and schema-known telemetry while transparently reporting latency, state-management, reliability, and transport trade-offs.
