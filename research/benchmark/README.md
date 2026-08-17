# Benchmark Workspace

`studies/` is the source of truth for reproducible results. Each study is
immutable and includes its manifest, Release test result, raw trials, processed
summaries, and graphs. Create one from the repository root:

```bash
python3 research/benchmark/scripts/run_study.py \
  --study-id phase0-linux-20260817 \
  --build-dir build/research \
  --iterations 100000 \
  --trials 30
```

The runner performs configure → build → complete CTest → benchmark → process →
plot. It stops before benchmarking when the Release test gate fails. A study
must have at least 30 trials, cannot overwrite an earlier study, and requires
a clean worktree unless explicitly marked as a development-only run.

- `scripts/`: study runner, raw capture, processing, and plotting code.
- `studies/`: complete, manifest-backed evidence packages.
- `raw_data/`, `processed/`, `graphs/`: preserved legacy artifacts only; see
  [legacy_data_status.md](legacy_data_status.md).

The current format baselines are explicitly **in-repository development
encoders**, not official third-party library measurements. Do not merge their
results with future official-library studies. See
[official_baselines.md](official_baselines.md) for the separate external-suite
requirements.
