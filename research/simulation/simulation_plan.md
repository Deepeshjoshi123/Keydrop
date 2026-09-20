# Simulation Plan

The dataset generator in `research/datasets/generate_datasets.py` creates deterministic CSV workloads for:

- Temperature sensors.
- Humidity sensors.
- GPS coordinates.
- Vehicle telemetry.
- Mixed telemetry.
- String-heavy payloads.
- Constant high-frequency streams.
- Variable high-frequency streams.
- Large payloads.

The current C++ benchmark executable uses a fixed `BenchmarkPayload`. To benchmark every generated dataset, add a dataset-driven benchmark executable that reads CSV rows, maps each row to a matching `SchemaDef`, and emits raw per-workload CSV. Until that executable exists, dataset files are workload specifications rather than measured benchmark inputs.

Required future benchmark columns:

- workload
- format
- trial
- row_count
- packet_size_bytes_mean
- encode_latency_ns_mean
- decode_latency_ns_mean
- throughput_per_sec
- allocations
- allocated_bytes
- bytes_per_second
- payload_reduction_percent
