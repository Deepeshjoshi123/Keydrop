# Keydrop

Adaptive Real-Time Telemetry Transport Engine written in C++.

## Overview

Keydrop is a high-performance telemetry transport runtime focused on:

- adaptive runtime optimization
- schema-aware packet minimization
- low-latency distributed communication
- reliability-aware packet processing

Unlike traditional serialization systems, Keydrop focuses on telemetry-oriented transport optimization and runtime adaptability.

---

## Build and Test

Keydrop uses CMake and C++17. The same source tree builds on Linux and Windows.

### Prerequisites

- CMake 3.20 or newer
- A C++17 compiler
  - Linux: GCC or Clang
  - Windows: Visual Studio 2022 (MSVC) or MinGW-w64

### Linux

Configure and build:

```bash
cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux --parallel
```

Run the complete test suite:

```bash
ctest --test-dir build/linux --output-on-failure
```

Run an individual transport test:

```bash
ctest --test-dir build/linux -R tcp_adapter --output-on-failure
```

Run the example:

```bash
./build/linux/bin/basic_encoding
```

Run the benchmark and save its report:

```bash
./build/linux/bin/benchmark_runner 4 build/linux/benchmark_report.txt
cat build/linux/benchmark_report.txt
```

### Windows with Visual Studio

Open a Developer PowerShell for Visual Studio, then configure and build:

```powershell
cmake -S . -B build/windows -G "Visual Studio 17 2022" -A x64
cmake --build build/windows --config Release --parallel
```

Run all tests:

```powershell
ctest --test-dir build/windows -C Release --output-on-failure
```

Run the example:

```powershell
.\build\windows\bin\Release\basic_encoding.exe
```

Run the benchmark and open its report:

```powershell
.\build\windows\bin\Release\benchmark_runner.exe 4 build\windows\benchmark_report.txt
Get-Content .\build\windows\benchmark_report.txt
```

### Windows with MinGW-w64

```powershell
cmake -S . -B build/mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build/mingw --parallel
ctest --test-dir build/mingw --output-on-failure
.\build\mingw\bin\benchmark_runner.exe 4 build\mingw\benchmark_report.txt
Get-Content .\build\mingw\benchmark_report.txt
```

`ctest` includes the unit tests, TCP/WebSocket adapter integration tests, and the benchmark-runner test. To run only benchmark-related tests, use:

```bash
ctest --test-dir build/linux -R benchmark --output-on-failure
```

On Windows, add `-C Release` when using the Visual Studio generator.

---

## Core Goals

- Reduce telemetry packet overhead
- Minimize repeated metadata transmission
- Optimize runtime packet encoding
- Support low-latency distributed systems
- Provide configuration-driven transport behavior

---

## Planned Features

- Binary packet encoding
- Adaptive schema compression
- Runtime packet optimization
- Transport abstraction layer
- Reliability validation
- Corruption detection
- Telemetry scheduling

---
## Long-Term Vision

Keydrop aims to become a lightweight adaptive telemetry transport runtime for modern distributed systems and real-time telemetry pipelines.

---

## License

MIT License
