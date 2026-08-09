# Experimental Methodology

This methodology is designed to make Keydrop experiments reproducible and publication-ready.

## Environment Capture

For every benchmark run, record:

- Hostname or machine identifier.
- CPU model, physical cores, logical cores, and base/boost frequency where available.
- RAM capacity and speed where available.
- Operating system name and version.
- Compiler name and version.
- CMake version.
- Git commit hash and dirty-worktree status.
- Build configuration, preferably Release.
- Date and local timezone.

## Build Configuration

Use a clean Release build:

```powershell
cmake -S . -B build/research -DCMAKE_BUILD_TYPE=Release
cmake --build build/research --parallel
ctest --test-dir build/research --output-on-failure
```

For Visual Studio generators, include `-C Release` when running tests and executables.

## Benchmark Procedure

1. Build the project in Release mode.
2. Run the full unit test suite.
3. Run a warm-up benchmark pass and discard it.
4. Run at least 30 repeated trials per workload.
5. Store each trial as raw CSV.
6. Process raw CSV into averages, standard deviations, minima, maxima, and confidence intervals where appropriate.
7. Generate graphs only from processed data.

## Measurements

Measure:

- Packet size in bytes.
- Encode latency in nanoseconds.
- Decode latency in nanoseconds.
- Throughput in operations per second.
- Logical allocation count.
- Logical allocated bytes.
- CPU usage when an external profiler is available.
- Memory usage when an external profiler is available.
- Bandwidth usage as bytes per message and bytes per second.
- Payload reduction relative to selected baseline.

## Statistical Treatment

Report mean, standard deviation, minimum, maximum, and sample count. Avoid drawing conclusions from a single run. For latency, include distribution plots or box plots if raw per-run data is available.

## Reproducibility Rules

- Never edit raw data by hand.
- Keep raw data, processed data, and graph scripts under `research/benchmark/`.
- Every graph must be regenerable from CSV files.
- Every numerical claim in the paper must cite a generated table or figure.
- When a measurement is unavailable, report it as unavailable rather than estimating it.
