#!/usr/bin/env bash
# Build DHB on Linux (Deepin/UOS/openKylin/Ubuntu).
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build}"
TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE="$TYPE"
cmake --build "$BUILD" -j"$JOBS"
ctest --test-dir "$BUILD" --output-on-failure

echo
echo "Built: $BUILD/dhb"
echo "Try:  $BUILD/dhb doctor"
