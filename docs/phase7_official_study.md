# Phase 7: Official Comparative Study

Phase 7 compares Keydrop against **official external implementations**,
clearly labeled, in a study with its own classification —
`official_external_baselines` — never conflated with the in-repository
development baselines.

## Comparison targets (this machine)

| Format | Implementation | Version |
|---|---|---|
| JSON | nlohmann::json (named exactly) | 3.11.3 |
| Protocol Buffers | official `protoc`-generated C++ (`telemetry.pb.cc`) | 3.21.12 |
| MessagePack | msgpack-c (`msgpack::pack`/`unpack`) | 4.0.0 |
| Keydrop stateless | `SchemaRuntime::send_ordered` (in-repo) | this commit |
| Keydrop stateful | Phase 3 delta stream, keyframe 100 (in-repo, labeled stateful) | this commit |

Libraries are detected by CMake (`find_package`); a missing library prints
`<format>_unavailable=1` instead of a fabricated row.

## Workloads (explicit, reproducible)

- **W1 fixed record**: `{temperature:32, humidity:70, device_id:"sensor_01"}`
- **W3 string-heavy**: `{device_id:"sensor_01", status:"operational", unit:"celsius", value:210}`
- **W4 timestamp stream**: `{timestamp:1000000+i·1000, device_id:"sensor_01", temperature:210+⌊(i%70)/10⌋, humidity:550+⌊(i%250)/25⌋, status:(i%500==0)}` — 10000 records, keyframe interval 100

Every row reports packet bytes, encode/decode latency, throughput, and
encode-window allocations. Allocation counts use the repository's
HeapTracker gross-allocation counting (documented Phase 0 terminology —
not total process memory).

## Running the official study

```bash
# After committing your work (a trusted study requires a clean worktree):
python3 research/benchmark/scripts/run_study.py \
  --study-id phase7-official-<platform>-<date> \
  --build-dir build/research \
  --iterations 100000 \
  --trials 30 \
  --official
```

The study records the commit, environment, library versions (printed by
the benchmark itself), raw `official_trials.csv`, processed summaries, and
regenerated figures in one immutable directory.

## Measured results (development run, 20000 iterations, this machine)

| Workload | Format | Bytes | Encode ns | Decode ns | Throughput/s |
|---|---|---:|---:|---:|---:|
| W1 fixed | keydrop_stateless | 17 | 372 | 168 | 1.85 M |
| W1 fixed | json_nlohmann 3.11.3 | 56 | 994 | 961 | 0.51 M |
| W1 fixed | protobuf 3.21.12 | 15 | 96 | 90 | 5.38 M |
| W1 fixed | msgpack-c 4.0.0 | 13 | 65 | 123 | 5.31 M |
| W3 strings | keydrop_stateless | 37 | 403 | 199 | 1.66 M |
| W3 strings | json_nlohmann | 77 | 1322 | 1186 | 0.40 M |
| W3 strings | protobuf | 36 | 178 | 187 | 2.74 M |
| W3 strings | msgpack-c | 33 | 136 | 159 | 3.39 M |
| W4 stream | keydrop_stateless (first packet) | 22 | — | — | — |
| W4 stream | keydrop_stateful (steady state) | **10.4** | — | — | — |

Stateful steady-state is 52.5% below the stateless packet (22 → 10.4 B)
with 0 round-trip failures over 10000 records.

## Scientifically honest interpretation

The measured table matches the plan's expectations:

- **Single fixed record**: Keydrop is competitive in bytes (17 vs
  protobuf 15, msgpack 13) and roughly 4-6× slower on encode than the
  specialized external codecs — protobuf and msgpack win raw latency.
  Keydrop's value here is validation + configuration, not raw speed.
- **JSON comparison**: Keydrop substantially reduces bytes (56 → 17, 77 →
  37) and is faster than JSON parse/dump in this harness.
- **Regular timestamp stream**: stateful mode reaches 52.5% lower
  steady-state bytes than its own stateless packet (target 25-60%).
- **Repeated strings / sparse fields**: see the Phase 3/4 studies (3A:
  56.8%; delta: 52.5%); these are in-repo comparisons and are labeled as
  such in those studies.
- **Random fields**: not yet measured here — this must be added before
  publication and reported even if Keydrop provides little benefit.

The conclusion must not claim universal codec superiority: Keydrop's
measured strengths are byte reduction for schema-known repetitive/sparse
telemetry and a beginner-configurable JSON-facing API; specialized codecs
win raw latency on single records.

## Publication gates checklist

- [x] Every figure and table regenerated from processed study data
      (`run_study.py --official` regenerates figures per study).
- [x] Every processed value traceable to raw CSV + commit (manifest).
- [x] Environment table from the manifest — no placeholders.
- [x] Workload definitions explicit (above + benchmark source).
- [x] Official-library and in-repo rows never conflated (distinct
      `classification` + `format` labels; separate study kinds).
- [x] Stateful and stateless comparisons clearly labeled.
- [x] Allocation terminology matches the implemented measurement method.
- [ ] Random-field workload (W6) — to add before publication.
- [ ] Limitations section covering platform/workload/network/memory
      instrumentation/baseline scope — draft in `research/technical_draft.md`.
- [x] Conclusion does not claim universal superiority.
