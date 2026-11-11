#!/usr/bin/env bash
set -euo pipefail

UPSTREAM_URL="https://github.com/angband/angband.git"
UPSTREAM_COMMIT="f3082213b73f3e463e3d0d60bff4b00462beae6e"
DEST="engine/upstream/angband"

if [ -d "$DEST/.git" ]; then
  git -C "$DEST" fetch --tags
else
  mkdir -p "$(dirname "$DEST")"
  git clone --filter=blob:none "$UPSTREAM_URL" "$DEST"
fi

git -C "$DEST" checkout --detach "$UPSTREAM_COMMIT"
test "$(git -C "$DEST" rev-parse HEAD)" = "$UPSTREAM_COMMIT"

echo "Angband 4.2.6 pinned at: $(git -C "$DEST" rev-parse HEAD)"
