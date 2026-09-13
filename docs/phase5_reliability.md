# Phase 5: Stateful Reliability and Recovery

Phase 5 completes the reliability story for stateful streams. Mechanisms
marked "existing" were built in earlier phases and are verified here.

## Mechanism checklist (plan)

| Mechanism | Status |
|---|---|
| Schema fingerprint/session handshake | Existing (Phase 1): `SchemaConfig::fingerprint` + `check_compatible`; the `keydrop_cli compatibility` command is the pre-stream handshake. |
| Dictionary reset/control messages | Existing (Phase 3): `[0xFA][0x00]` via `send_dictionary_reset`, applied by `receive_stream`. |
| Sequence numbers | Existing (Phase 3): per-schema delta sequences; a mismatch is rejected. |
| Periodic full keyframes | Existing (Phase 3): `keyframe_interval`; any full packet resynchronizes. |
| Configurable checksum/CRC | **New**: `ReliabilityConfig::enable_crc32` wraps every stream packet in `[0xF9][crc32 LE][payload]`; `receive_stream` verifies and strips the envelope. A mismatch returns `corruption_detected` and nothing is decoded. |
| Safe packet resynchronization | Existing: `PacketSynchronizer` + `receive_recovered_stream`; rejected deltas never partially decode. |
| Decoder memory limits | **New**: `ReliabilityConfig::max_recovered_packets` caps how many packets `receive_recovered_stream` will decode (default 256). Existing bounds: dictionary `max_entries` (FIFO eviction), batch queue ≤ `max_batch_packets`, payload/buffer pools ≤ `max_available`. |
| Unknown-schema/version rejection | Existing: unknown `message_id` → `schema_not_found`; version/name/id mismatches → `check_compatible` failures with specific reasons. |
| Backward-compatible schema evolution policy | **New (documented + enforced)**: the fingerprint now covers field name, type, stable field id, and constraints. Any field-list, type, order, id, or constraint change makes the fingerprint differ and the handshake fails — wire-incompatible changes are never silently decoded. Evolution requires a version bump (or new schema name/id) and a fresh handshake. |

## CRC32 envelopes

```cpp
ReliabilityConfig reliability;
reliability.enable_crc32 = true;
reliability.max_recovered_packets = 256;
runtime.set_reliability_config(reliability);
// send_stream() / flush_stream() now emit [0xF9][crc32][payload];
// receive_stream() verifies and strips the envelope.
```

- The CRC covers the full stream packet (full, delta, or batch envelope).
- `CorruptionDetector::crc32` gained a raw-pointer overload (used by the
  envelope) alongside the existing `Buffer` overload and `check_packet`
  options (`enable_crc32` + `crc32_offset` for custom packet layouts).
- The dictionary-reset control packet is not wrapped; the envelope is
  stripped before control dispatch, and the control packet itself remains
  `[0xFA][0x00]`.
- `PacketSynchronizer` recovery operates on unwrapped streams; CRC-wrapped
  streams use ordered `receive_stream`. Documented limitation below.

## Verification

| Gate | Target | Result |
|---|---:|---:|
| Corrupted packet accepted as valid | 0 | CRC mismatch → `corruption_detected`, nothing decoded (test-pinned; bit-flipped envelopes rejected) |
| Stateful desynchronization silently misdecodes payload | 0 | Phase 3 sequence checks + loss-safety benchmark gate (0 misdecodes with drops) |
| Recovery after configured keyframe interval | 100% | Phase 3 tests: drop → reject → keyframe → resync |
| Fuzz-test crash or out-of-bounds read | 0 | `test_fuzz_reliability`: 20k random buffers + 20k mutated valid packets (bit flips, truncation, extension, overwrite) + 2k envelope-prefixed garbage through every receive path — clean under **ASan + UBSan** |
| Compatibility test failures | 0 | `test_compatibility`: name/id/version/order/type/constraint mismatches all rejected with specific reasons; evolution (appended field) rejected; unknown schema/id rejection |

All 31 ctest tests pass in Release; the fuzz, compatibility, delta, adaptive,
and core tests also pass under AddressSanitizer + UndefinedBehaviorSanitizer.

## Limitations

- CRC envelopes and `PacketSynchronizer` resynchronization do not compose:
  recovery scans for unwrapped packet layouts. Use ordered `receive_stream`
  for CRC-wrapped streams, or unwrap before feeding a byte stream to
  `receive_recovered_stream`.
- The envelope adds 5 bytes per stream packet; enable it only where
  corruption detection is required (opt-in, like the plan's other
  advanced settings).
- `receive_recovered_stream` decodes at most `max_recovered_packets`
  packets; additional recoverable packets in the stream are left
  undecoded (the caller may re-invoke with the remaining bytes).
