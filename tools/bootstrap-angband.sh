#/usr/bin/env bash
set -euo pipefail

UPSTREAM_URL="https://github.com/angband/angband.git"
UPSTREAM_COMMIT="42cd156ac9f95ddf4311e88dd7ad462292d6d1d6"
DEST="engine/upstream/angband"

if [ -d "$DEST/.git" ]; then
  git -C "$DEST" fetch --all --tags
else
  git clone "$UPSTREAM_URL" "$DEST"
fi

git -C "$DEST" checkout --detach "$UPSTREAM_COMMIT"
echo "Angband pinned at: $(git -C "$DEST" rev-parse HEAD)"
