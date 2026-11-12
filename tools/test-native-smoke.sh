#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ABFP_BUILD_DIR:-$ROOT/build-angband}/game/angband"

if [[ ! -x "$BIN" ]]; then
  echo "Missing native Angband executable: $BIN" >&2
  exit 1
fi

# The -l startup path opens the configured save directory.  Give the test an
# isolated, writable save directory rather than relying on the runner's system
# paths or per-user config directory.
TMP_HOME="$(mktemp -d)"
trap 'rm -rf "$TMP_HOME"' EXIT
mkdir -p "$TMP_HOME/save"

HOME="$TMP_HOME" "$BIN" "-dsave=$TMP_HOME/save" -l

echo "ABFP native Angband executable smoke test passed."
