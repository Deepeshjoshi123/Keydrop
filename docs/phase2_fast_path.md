# Phase 2: Stateless Schema-Aware Fast Path

The fast path is a per-schema specialized codec (`FastCodec`) that is
precomputed when a schema is registered and cached inside `SchemaRegistry`.
It removes the generic-runtime machinery from the hot loop while the general
`SchemaRuntime::send`/`receive` paths remain available, unchanged, for
dynamic or untrusted inputs.

## API

```cpp
// Encode schema-ordered typed values. out_packet is cleared and reused:
// its reserved capacity is kept across calls (encode_into semantics).
SchemaRuntimeResult SchemaRuntime::fast_encode(
    const std::string& schema_name,
    const FieldValue* values,
    usize count,
    Buffer& out_packet) const;

// Decode in place. Numeric fields are promoted into FastDecodedField;
// string/bytes fields are returned as borrowed BufferView values that
// point into `packet` and remain valid while `packet` is alive.
SchemaRuntimeResult SchemaRuntime::fast_decode(
    const Buffer& packet,
    std::string& out_schema_name,
    FastDecodedField* out_fields,
    usize max_fields,
    usize& out_count) const;
```

```cpp
FieldValue values[3] = {
    FieldValue::from_u16(23), FieldValue::from_u16(71),
    FieldValue::from_string("sensor-01"),
};
Buffer packet;
runtime.fast_encode("SensorReading", values, 3, packet);   // reuses packet's capacity

FastDecodedField fields[3];
std::string schema_name;
usize count = 0;
runtime.fast_decode(packet, schema_name, fields, 3, count);
// fields[2].view points into packet — zero-copy; keep packet alive while using it
```

## What the fast path does and does not do

Does:

- Direct per-field writes/reads through function pointers fixed at
  registration time — no map lookups, no temporary `FieldValue` copies,
  no generic validation walk, no intermediate packets.
- Encode into the caller's `Buffer` (capacity reuse across calls).
- Decode strings/bytes as zero-copy `BufferView` values into the packet.
- Accept plain stateless packets and `RuntimeOptimizer`-optimized packets
  (`0xFD` marker), decoding the bitmap in place.
- Resolve adaptive-dictionary string references; the resolved value is
  materialized in `FastDecodedField::owned` with `owned_string = true`
  (there is nothing to borrow from the packet in that case).
- Keep every read bounds-checked: truncated or malformed input returns an
  error, never an out-of-bounds access.

Does not:

- Run `SchemaValidator` payload checks or the `CorruptionDetector` walk —
  use the general path for untrusted inputs.
- Apply `RuntimeOptimizer` zero-value omission on encode — fast packets are
  the plain stateless format, byte-identical to the general path with the
  optimizer disabled.
- Decode stream batch envelopes (`0xFC`) — `receive_stream()` handles those.

## Measuring against the Phase 2 targets

```bash
./build/bin/fastpath_benchmark 100000
```

The benchmark compares the general path (the fixed-record baseline used by
`benchmark_runner`) with the fast path on the 3-field benchmark record.
Baseline encode constructs its `OrderedPayload` per iteration exactly like
the existing repository benchmark; fast encode models the steady-state
caller pattern (reused `FieldValue` array with in-place numeric updates,
reused output buffer). Allocation counts cover the encode window only,
matching the existing repository benchmark terminology.

Phase 2 engineering targets (measured, not guaranteed):

| Metric | Target |
|---|---:|
| Packet size | no regression; byte-identical to baseline |
| Encode latency | 20-35% lower |
| Decode latency | 20-40% lower |
| Throughput | 25-60% higher |
| Allocations per encode | 70-90% lower |
| Allocated bytes per encode | 50-80% lower |
| Round-trip failures | 0 |

## Measured results (development runs)

`fastpath_benchmark 200000`, Release build on this machine, three repeated
runs. These are development measurements, not a manifest-backed study —
publishable numbers must come from `scripts/run_study.py` with the
fast-path benchmark added to a versioned study.

| Metric | Target | Measured range |
|---|---|---:|
| Packet size | no regression | byte-identical (17 B) |
| Encode latency | 20-35% lower | 57-79% lower |
| Decode latency | 20-40% lower | 37-41% lower |
| Throughput | 25-60% higher | 97-210% higher |
| Allocations per encode | 70-90% lower | 100% lower (5 → 0/op) |
| Allocated bytes per encode | 50-80% lower | 100% lower (370 → 0 B/op) |
| Round-trip failures | 0 | 0 |

The fast path reaches zero allocations per operation because the caller owns
all storage: the `FieldValue` array, the output buffer (capacity reused via
`encode_into` semantics), and the schema-name string. Note that a 16+ char
schema-name literal re-allocates a `std::string` per call at the call site —
pre-build the name once, as the benchmark does.
