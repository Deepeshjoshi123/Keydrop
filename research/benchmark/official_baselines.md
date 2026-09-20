# Official External Baselines

The existing `benchmark_runner` is an **in-repository development baseline**.
Its `json`, `protobuf`, and `messagepack` labels are local implementations and
must never be presented as results from official third-party libraries.

## Implemented (Phase 7)

`official_baselines_benchmark` compares against the official libraries,
detected by CMake and version-labeled in every output row:

- JSON: nlohmann::json (named exactly, version from its headers);
- Protocol Buffers: `protoc`-generated C++ for `benchmarks/proto/telemetry.proto`;
- MessagePack: msgpack-c (`libmsgpack-dev`); unavailable libraries print
  `<format>_unavailable=1` instead of fabricated rows.

The study runner classifies runs with `--official` as
`official_external_baselines` and writes `official_trials.csv` beside the
in-repository CSVs, with distinct `format=` labels per row. In-repository
rows inside an official study are labeled `keydrop_stateless` /
`keydrop_stateful` — the two classifications are never merged.

Workload definitions, measured tables, and interpretation guidance:
`docs/phase7_official_study.md`.

Do not add official-library rows to an `in-repository development baselines`
study, and do not use in-repo rows to make claims about external libraries.
