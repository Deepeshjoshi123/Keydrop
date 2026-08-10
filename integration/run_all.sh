#!/usr/bin/env bash
# Build and run all integration examples in order.
#
# Usage:  ./run_all.sh
#
# The TCP server/client demo is interactive — the script starts
# the server in the background, runs the client, and cleans up.

set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build"

echo "=============================================="
echo " Keydrop Integration Examples — Build & Run"
echo "=============================================="

# ---- Build -----------------------------------------------------
echo ""
echo "[1/6] Configuring and building all examples..."
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build "$BUILD" --target all_integration --parallel
echo "      Build complete."

# ---- 01: send-receive ------------------------------------------
echo ""
echo "[2/6] 01-send-receive — basic encode/decode"
"$BUILD/integration/01-send-receive"

# ---- 02: dictionary --------------------------------------------
echo ""
echo "[3/6] 02-with-dictionary — adaptive string dedup"
"$BUILD/integration/02-with-dictionary"

# ---- 03: stream batching ---------------------------------------
echo ""
echo "[4/6] 03-stream-batching — reuse + batch envelopes"
"$BUILD/integration/03-stream-batching"

# ---- 04: TCP ---------------------------------------------------
echo ""
echo "[5/6] 04-tcp — client/server transport"
"$BUILD/integration/04-tcp-server" &
SERVER_PID=$!
sleep 0.3
"$BUILD/integration/04-tcp-client"
wait "$SERVER_PID" 2>/dev/null || true

# ---- 05: corruption recovery -----------------------------------
echo ""
echo "[6/6] 05-corruption-recovery — packet recovery from noise"
"$BUILD/integration/05-corruption-recovery"

# ---- Done ------------------------------------------------------
echo ""
echo "=============================================="
echo " All examples passed."
echo "=============================================="
