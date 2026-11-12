#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="\${ABFP_BUILD_DIR:-$ROOT/build-angband}/game/angband"

if [[ ! -x "$BIN" ]]; then
  echo "Missing native Angband executable: $BIN" >&2
  exit 1
fi

python3 "$ROOT/tools/test-host-listener.py" "$BIN"
