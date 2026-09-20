# Reproducible Benchmark Studies

Each directory in this folder is an immutable benchmark study. A study keeps
the source revision, dirty-worktree state, environment, exact commands, full
Release test log, raw trial output, processed summaries, figure inputs, and
generated figures together.

Commit or otherwise freeze the source first, then create a trusted study from
the repository root:

```bash
python3 research/benchmark/scripts/run_study.py \
  --study-id phase0-linux-20260817 \
  --build-dir build/research \
  --iterations 100000 \
  --trials 30
```

The command refuses to reuse a study identifier and rejects dirty worktrees.
It configures a Release build, builds it, runs the complete CTest suite,
captures 30 or more trials, then processes and plots only that study's raw
data. Use `--allow-dirty` only for development validation; its manifest is
explicitly marked ineligible for publication.

Do not hand-edit `raw/`, `processed/`, or `graphs/`. The corresponding
provenance files record input hashes and source revision. Historical material
under `raw_data/`, `processed/`, and `graphs/` predates this layout and is not
publication evidence unless it is reconciled into a manifest-backed study.
