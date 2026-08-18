# Phase 4: Adaptive Profiles and Explicit Configuration

Phase 4 adds named configuration profiles and windowed adaptive refinement
on top of the Phase 2/3 machinery.

## Profiles

```yaml
profile: telemetry-low-latency
profile: telemetry-balanced
profile: telemetry-bandwidth
profile: telemetry-lossless-archive
```

| Profile | Dictionary | Zero-value omission | Stream | Deltas | Batching | Adaptive |
|---|---|---|---|---|---|---|
| telemetry-low-latency | off | on | off | off | off | no |
| telemetry-balanced | on | on | reuse + batching | off | on (≤4) | yes |
| telemetry-bandwidth | on | on | reuse | on (keyframe 100) | off | yes |
| telemetry-lossless-archive | on | on | reuse | on (keyframe 50) | on (≤4) | yes |

- The YAML loader validates the profile name and rejects unknown profiles
  (`validate`/`encode`/`decode` fail with the supported list).
- `apply_profile(runtime, profile)` / `apply_configured_profile(runtime,
  schema)` apply the profile without marking components as explicitly
  configured, so adaptive refinement may still adjust them.
- Profiles are a starting point: the adaptive layer refines them.

## Adaptive decisions

`AdaptiveProfiler` observes every successful send, accumulates per-schema
statistics in windows (`window_size`, default 100), and at each window
boundary applies the plan's decision rules:

```text
Enable dictionary when repeated-string ratio >= 60%
and predictive hit rate >= 80%.

Enable change-only/delta packets when the unchanged-field ratio >= 40%.
```

- The dictionary hit rate is measured predictively: the fraction of string
  values in the current window that appeared in the previous window (the
  first window bootstraps from within-window repetition).
- Delta encoding is attempted only when the unchanged ratio meets the
  threshold; the per-record "smaller than the full representation" check
  from Phase 3 still disables itself when not repaid.
- Batching remains profile-managed and count-bounded; see the Phase 3 note
  on queueing delay.
- **Explicit user configuration always wins.** `set_dictionary_config`,
  `set_optimizer_config`, and `set_stream_optimizer_config` mark the
  component as explicit, and the adaptive layer never overrides it.
- Toggling the delta mode forces a keyframe on the next emission
  (`reset_delta_state`) while keeping queued batches, closing the
  stale-base acceptance window.

No general compression is enabled for small telemetry messages.

## API

```cpp
// explicit profile application
ProfileSettings profile;
try_get_profile("telemetry-bandwidth", profile);
apply_profile(runtime, profile);

// adaptive refinement (opt-in; profiles marked adaptive do this via
// apply_configured_profile)
AdaptiveProfilerConfig adaptive;
adaptive.enabled = true;
adaptive.window_size = 100;
runtime.set_adaptive_config(adaptive);

// from YAML
apply_configured_profile(runtime, configured_schema);
```

## Measured results (development runs)

`adaptive_profiles_benchmark`, 10000 records, window 100, Release build.

| Gate | Target | Measured |
|---|---:|---:|
| Suitable stream: lower amortized bytes than fixed mode | 20-60% | **66.5%** |
| Unsuitable stream: packet-size overhead | ≤ 5% | **-0.13%** (slightly smaller) |
| Incorrect automatic choice | < 5% of windows | **0%** (0 wrong windows both streams) |
| Lossless reconstruction | 100% | **100%** (10000/10000 both streams, 0 misdecodes) |
| Explicit user override honored | 100% | 100% (covered by `test_adaptive_profiler`) |

Development measurements only — publication numbers require a
manifest-backed study.

## Bugs found and fixed while implementing Phase 4

- `RuntimeOptimizer` treated a dictionary-reference marker (`0xFFFF` length
  prefix) as a literal 65535-byte string and failed to encode packets that
  contained dictionary references — a Phase 3 blind spot exposed by the
  balanced profile (dictionary + zero-value omission both on). Both the
  optimize and deoptimize paths now treat string `0xFFFF` as a fixed 4-byte
  reference, matching the corruption detector and packet synchronizer.
- The adaptive first-window apply originally fired before any window
  completed; it is now gated on an actually-completed window.

## Limitations

- Decisions are runtime-wide (the last completed window wins across
  schemas); per-schema divergence is future work.
- Profile application is runtime-level; multi-schema YAML files with
  different profiles apply the last one.
- The adaptive layer observes `send()` (and therefore `send_stream()`),
  not `send_ordered()` or the fast path.
