#!/usr/bin/env bash
# Build Keydrop in Release mode and run the format benchmark.
#
# Usage:  ./scripts/run_benchmark.sh [iterations]
#         default iterations = 100000

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ITERATIONS="${1:-100000}"

echo "=== Building Keydrop (Release) ==="
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build "$ROOT/build" --target benchmark_runner --parallel

echo ""
echo "=== Running benchmark (${ITERATIONS} iterations) ==="
"$ROOT/build/bin/benchmark_runner" "$ITERATIONS" "$ROOT/benchmark_report.txt"

echo ""
echo "=== Report saved to benchmark_report.txt ==="
