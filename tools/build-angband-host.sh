#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
bash "$ROOT/tools/bootstrap-angband.sh"

BUILD_DIR="${ABFP_BUILD_DIR:-$ROOT/build-angband}"
cmake -S "$ROOT/engine/upstream/angband" -B "$BUILD_DIR" \
  -DSUPPORT_GCU_FRONTEND=OFF \
  -DSUPPORT_X11_FRONTEND=OFF \
  -DSUPPORT_SPOIL_FRONTEND=OFF \
  -DSUPPORT_BORG=OFF
cmake --build "$BUILD_DIR" --parallel "${ABFP_BUILD_JOBS:-2}"

echo "Angband host built at: $BUILD_DIR/game/angband"
