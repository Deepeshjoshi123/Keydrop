# Benchmark Workspace

This directory separates generated data from source scripts.

- `scripts/`: reproducible benchmark, parsing, processing, plotting, and diagram helpers.
- `raw_data/`: raw CSV files from benchmark runs.
- `processed/`: aggregated CSV files derived from raw data.
- `graphs/`: generated PNG and SVG figures.

Run from the repository root:

```powershell
python research\datasets\generate_datasets.py
python research\benchmark\scripts\run_benchmarks.py --build-dir build\research --iterations 1000 --trials 30
python research\benchmark\scripts\process_results.py
python research\benchmark\scripts\plot_graphs.py
python research\diagrams\generate_diagrams.py
```

The scripts do not invent missing results. If raw CSV files are absent, processing or plotting exits with a clear error.
