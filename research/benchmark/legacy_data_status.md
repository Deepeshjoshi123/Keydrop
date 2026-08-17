# Historical Benchmark Artifact Status

The files in `raw_data/`, `processed/`, and `graphs/` were created before the
manifest-backed study workflow. They are preserved as development history, but
they must not be used as publication evidence because their exact environment,
source revision, build/test result, selected raw inputs, and figure provenance
are not recorded together.

The legacy `format_summary.csv` also aggregates matching files from different
runs, so a row cannot be tied to a single frozen source state. New reported
results must be generated with `scripts/run_study.py`; its own study directory
is the complete evidence package.

The current format labels `json`, `protobuf`, and `messagepack` refer to the
in-repository development encoders in `src/benchmark/format_benchmark.cpp`.
They are not measurements of official JSON, Protocol Buffers, or MessagePack
libraries. Official-library experiments require separate dependencies,
workloads, and manifest-backed studies before making external comparisons.
