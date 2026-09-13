# Phase 3: Stateful Bandwidth Optimization

Phase 3 adds a stateful stream mode on top of the Phase 2 stateless fast
path. Everything here is opt-in (`StreamOptimizerConfig::enable_delta_packets`
defaults to false), so existing stateless and batched behavior is unchanged
unless the caller enables it.

## 3A. Repeated-string dictionary

`AdaptiveDictionary` (existing): bounded, resettable dictionary references.
A string that repeats is replaced on the wire by the marker `0xFFFF` + a
2-byte ID after its first appearance. The decoder learns inline strings, so
the two sides stay consistent without negotiation.

Phase 3 additions:

- **Dictionary reset control packet**: `send_dictionary_reset(Buffer&)`
  emits `[0xFA][0x00]`; `receive_stream()` recognizes it and resets the
  local dictionary. Use it on reconnect or state reset.
- **Safe rejection of stale references**: after a reset, a packet carrying
  an old reference fails decoding (`decode_failed`) — it is never
  misdecoded.

## 3B + 3C. Presence bitmap and delta coding

New delta packet format (marker `0xFB`), built and expanded by
`StreamOptimizer` when `enable_delta_packets` is on:

```
[0xFB][message_id u16 LE][seq u16 LE][bitmap_size u8][bitmap...][fields]
```

- The bitmap marks changed fields; unchanged fields are copied from the
  receiver's last decoded payload (3B).
- Changed integer fields are encoded as signed deltas in the smallest of
  1/2/4 bytes with a 1-byte size tag, with a raw full-width fallback for
  deltas that overflow (3C). u8/i8 fields are always raw 1 byte.
- f32/f64 ride the bitmap only (float deltas never repay their tag).
- Changed string/bytes fields carry their full value (no dictionary
  references inside delta packets).
- A delta packet is emitted only when it is smaller than the full stateless
  packet — the optimization disables itself when its overhead is not repaid.

**Sequence numbers and keyframes**:

- The first record for a schema is always a full packet (initial keyframe).
- Every `keyframe_interval` records, a full packet is emitted (periodic
  keyframe). Any full packet — keyframe or delta-not-repaid fallback —
  resynchronizes both sender and receiver sequence counters.
- Delta packets carry a per-schema sequence counter. A missing or reordered
  delta (loss) makes the next delta fail its sequence check and be rejected:
  **stateful desynchronization can never silently misdecode a payload**.
  The stream recovers automatically at the next full packet (keyframe).

Config:

```cpp
StreamOptimizerConfig cfg;
cfg.enabled = true;
cfg.enable_delta_packets = true;  // opt-in stateful mode
cfg.keyframe_interval = 100;      // full packet every N records
cfg.low_change_ratio_threshold = 0.5f; // engage only for low-change streams
runtime.set_stream_optimizer_config(cfg);
```

API: `send_stream` / `receive_stream` as before; `flush_stream` unchanged.
`send_dictionary_reset` emits the control packet.

## 3D. Batch framing

Batching (existing `0xFC` envelopes) is count-based (`max_batch_packets`).
**Measured result (negative)**: the current batch envelope stores each
record's full packet including its 2-byte message_id, so the per-record
framing (2-byte message_id + 2-byte batch length) never beats plain packets
on serialization bytes — the benchmark measures -11% on the fixed record.
Batching's value is at the transport layer (fewer sends, less per-send
framing), not in serialization bytes. A future shared-header batch format
(one message_id per batch, per-record lengths only) is required to reach
the plan's 3D byte targets. This negative result is retained per the
research-integrity rules.

## Measured results (development runs)

`stateful_bandwidth_benchmark`, 10000 records, Release build. Workload W:
22-byte fixed record (timestamp u32, device_id string, status u8,
temperature u16, humidity u16) with slowly changing values; workload W3:
37-byte string-heavy record. Development measurements only — publication
numbers require a manifest-backed study.

| Section | Gate / target | Measured |
|---|---:|---:|
| 3A dictionary (W3) | 35-45% steady-state reduction | **56.8%** (37 B → ~16 B steady) |
| 3A dictionary (mixed W) | — | 31.8% (22 B → 15 B steady) |
| 3A stale reference | never silently misdecoded | rejected with `decode_failed` |
| 3B + 3C delta (W) | 25-70% reduction | **52.5%** (22 B → 10.4 B steady) |
| 3B + 3C delta (W3) | — | 79.7% |
| 3C delta-not-repaid | disable when ineffective | full packet emitted instead |
| Loss: silent misdecode | 0 | **0** (all rejects recovered at keyframe) |
| Loss: recovery | after keyframe interval | recovers at next full packet |
| 3D batching (W) | 5-15% reduction | **-11%** (negative; see above) |

## Limitations

- Delta mode is per-schema stateful; sender and receiver must stay in
  lockstep. Any full packet resynchronizes, so loss degrades bandwidth
  (rejections) until the next keyframe — never correctness.
- Delta packets do not carry dictionary references; strings inside delta
  packets are full values (unchanged strings cost nothing via the bitmap).
- Batching has no time-based latency bound in this implementation; the
  maximum queueing delay is bounded by `max_batch_packets` × the caller's
  record period. A byte reduction must not violate a configured maximum
  delay — callers with strict latency budgets should use delta mode (which
  emits immediately) or disable batching.
