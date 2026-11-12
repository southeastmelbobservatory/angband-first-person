#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

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

PATCH="$REPO_ROOT/engine/upstream/patches/angband-4.2.6-abfp.patch"
if git -C "$DEST" apply --check "$PATCH"; then
  git -C "$DEST" apply "$PATCH"
elif git -C "$DEST" apply --reverse --check "$PATCH"; then
  echo "ABFP Angband frontend patch already applied"
else
  echo "ABFP patch does not apply cleanly to the pinned Angband revision" >&2
  exit 1
fi

echo "Applied ABFP frontend integration patch"
