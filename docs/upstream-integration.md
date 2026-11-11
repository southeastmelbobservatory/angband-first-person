# Upstream Integration Plan

## Baseline

Use the official `angband/angband` repository and pin a specific commit before writing version-sensitive adapter code.

## Integration phases

1. Build upstream unchanged.
2. Identify the frontend/input command boundary.
3. Export a bounded local snapshot around the player.
4. Instrument authoritative movement.
5. Instrument combat only at real resolution points.

## Movement event rule

1. Record old authoritative position.
2. Execute the Angband command.
3. Read resulting authoritative position.
4. Emit `ENTITY_MOVED` only when the position actually changed.

The first proof needs terrain, player position, one visible monster, one movement command, a refreshed snapshot and an ordered movement event.
