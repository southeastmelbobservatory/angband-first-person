#!/usr/bin/env bash
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

missing=()
for cmd in gcc cmake pkg-config; do
  command -v "$cmd" >/dev/null 2>&1 || missing+=("$cmd")
done
if command -v pkg-config >/dev/null 2>&1; then
  pkg-config --exists ncurses 2>/dev/null || pkg-config --exists ncursesw 2>/dev/null || missing+=("libncurses-dev")
fi

if [ "${#missing[@]}" -gt 0 ]; then
  echo "Missing Linux build prerequisites in WSL: ${missing[*]}"
  echo ""
  echo "Install them once inside WSL with:"
  echo "  sudo apt update && sudo apt install -y build-essential cmake pkg-config libncurses-dev"
  echo ""
  read -r -p "Press Enter to close..."
  exit 1
fi

bash "$ROOT/tools/run-native-demo.sh" "$@"
status=$?
if [ "$status" -ne 0 ]; then
  echo ""
  echo "Angband host exited with code $status."
  read -r -p "Press Enter to close..."
fi
exit "$status"
