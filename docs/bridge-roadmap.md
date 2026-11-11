# Engine Bridge Roadmap

## M1 — Upstream baseline
- [x] Pin the official Angband 4.2.6 release commit.
- [ ] Bootstrap and build the pinned upstream checkout in a clean environment.

## M2 — Command bridge
- [x] Translate eight directional moves to `CMD_WALK` and keypad directions.
- [x] Translate wait to `CMD_HOLD`.
- [ ] Connect submission to a real Angband frontend/game-loop host and verify turn processing.

## M3 — Snapshot bridge
- [x] Export a 21x21 player-centred snapshot.
- [x] Export known terrain, visibility and lighting.
- [x] Gate monster/object occupancy behind current visibility.
- [ ] Add richer terrain identifiers, traps, shops and stairs metadata.
- [ ] Export visibility-safe monster and object entity records.

## M4 — Event bridge
- [x] Subscribe to `EVENT_PLAYERMOVED`.
- [x] Emit authoritative player movement coordinates.
- [x] Refresh the local snapshot after player movement.
- [ ] Add combat, damage, death, object and terrain events at their authoritative resolution points.

## M5 — Diagnostic frontend
- [ ] Start Angband through a host frontend.
- [ ] Queue one ABFP movement command.
- [ ] Confirm the normal Angband loop processes the turn.
- [ ] Compare before/after snapshots and movement event.
- [ ] Verify blocked movement emits no player-moved event.

## Definition of success

The player presses Forward, Angband processes a normal turn, and the renderer receives a fresh 21x21 view. The camera moves only when Angband's authoritative player position changes.
