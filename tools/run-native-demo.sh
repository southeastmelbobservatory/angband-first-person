#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
bash "$ROOT/tools/build-angband-host.sh"

cd "$ROOT/build-angband/game"
echo "ABFP native host starting."
echo "Complete normal Angband character setup in this terminal."
echo "Once you enter the dungeon, connect the first-person client to 127.0.0.1:47652."
exec ./angband "$@"
