#!/usr/bin/env bash
# Build and run the TCP client-server demo.
#
# Starts the server in the background, runs the client, waits, and cleans up.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="$ROOT/build"

echo "=== Building integration examples ==="
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build "$BUILD" --target 04-tcp-server 04-tcp-client --parallel

echo ""
echo "=== Starting TCP server (background) ==="
"$BUILD/integration/04-tcp-server" &
SERVER_PID=$!
sleep 0.3

echo ""
echo "=== Running TCP client ==="
"$BUILD/integration/04-tcp-client"

wait "$SERVER_PID" 2>/dev/null || true
echo ""
echo "=== Demo complete ==="
