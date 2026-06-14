#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="${BUILD_DIR:-build}"
DATASET="${DATASET:-datasets/coauth-MAG-History-full.hyp}"
OUTPUT_DIR="${DHYP_OUTPUT_DIR:-outputs}"

generator_args=()
if [[ -n "${CMAKE_GENERATOR:-}" ]]; then
  generator_args=(-G "$CMAKE_GENERATOR")
elif command -v ninja >/dev/null 2>&1; then
  generator_args=(-G Ninja)
fi

cmake -S . -B "$BUILD_DIR" "${generator_args[@]}" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel

BIN="$BUILD_DIR/DynamicHypergraphCore"
if [[ ! -x "$BIN" && -x "$BIN.exe" ]]; then
  BIN="$BIN.exe"
fi

mkdir -p "$OUTPUT_DIR"
export DHYP_OUTPUT_DIR="$OUTPUT_DIR"

echo "[1/5] Dataset information"
"$BIN" "$DATASET" info

echo "[2/5] Static neighbor-core decomposition"
"$BIN" "$DATASET" run 0 1 --info

echo "[3/5] Single-edge dynamic update examples"
"$BIN" "$DATASET" delE_single 1
"$BIN" "$DATASET" addE_single 10,11,12

echo "[4/5] Batch dynamic update examples"
"$BIN" "$DATASET" delE_batch datasets/coauth-MAG-History-delete-edges.txt
"$BIN" "$DATASET" addE_batch datasets/coauth-MAG-History-insert-edges.hyp

echo "[5/5] Help page"
"$BIN" --help

echo "Done. Outputs are under $OUTPUT_DIR/."
