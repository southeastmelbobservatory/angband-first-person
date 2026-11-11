# Engine Bridge Roadmap

## M1 — Upstream baseline

- pin Angband 4.2.6 or a chosen upstream commit
- build unchanged
- document the exact revision

## M2 — Command bridge

Support wait and eight directional movement commands through Angband's existing command queue.

## M3 — Snapshot bridge

Export terrain, visibility, player, monsters and objects around the player.

## M4 — Event bridge

Map authoritative changes into movement, combat, damage, death and terrain events.

## Definition of success

The player presses Forward, Angband processes a normal turn, and the 3D camera moves only if the authoritative player position changed.
