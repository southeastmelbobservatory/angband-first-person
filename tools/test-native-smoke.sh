#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ABFP_BUILD_DIR:-$ROOT/build-angband}/game/angband"

if [[ ! -x "$BIN" ]]; then
  echo "Missing native Angband executable: $BIN" >&2
  exit 1
fi

# Exercise the real linked executable and startup/data-path initialization
# without requiring an interactive ncurses terminal.
"$BIN" -l

echo "ABFP native Angband executable smoke test passed."
