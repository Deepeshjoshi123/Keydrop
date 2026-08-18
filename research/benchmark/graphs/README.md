# Benchmark Graphs

These figures are regenerated copies of the study
`research/benchmark/studies/phase7-official-20260818/graphs/`. The study
directory is the authoritative evidence package (manifest, raw CSV,
processed summaries, provenance, figures together). Regenerate with:

```bash
python3 research/benchmark/scripts/run_study.py \
  --study-id phase7-official-<platform>-<date> \
  --build-dir build/research --iterations 100000 --trials 30 --official
```

Figures: packet size comparison, encoding/decoding latency, throughput,
memory behavior, payload reduction, and memory allocation counts. The
`official_trials.csv` rows (JSON/nlohmann, protobuf, msgpack, Keydrop
stateless + stateful) are processed into `official_summary.csv`.
