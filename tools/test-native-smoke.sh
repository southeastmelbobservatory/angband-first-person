#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ABFP_BUILD_DIR:-$ROOT/build-angband}/game/angband"

if [[ ! -x "$BIN" ]]; then
  echo "Missing native Angband executable: $BIN" >&2
  exit 1
fi

# Angband expects a writable per-user directory even for its save-list query.
# Keep smoke-test state isolated from the runner account and repository.
TMP_HOME="$(mktemp -d)"
trap 'rm -rf "$TMP_HOME"' EXIT
mkdir -p "$TMP_HOME/user"

HOME="$TMP_HOME" "$BIN" "-duser=$TMP_HOME/user" -l

echo "ABFP native Angband executable smoke test passed."
