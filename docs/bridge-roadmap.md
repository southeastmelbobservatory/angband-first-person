# Engine Bridge Roadmap

## M1 — Upstream baseline
- [x] Pin the official Angband 4.2.6 release commit.
- [ ] Bootstrap and build the pinned upstream checkout in a clean environment.

## M2 — Command bridge
- [x] Translate eight directional moves to CMD_WALK and keypad directions.
- [x] Translate wait to CMD_HOLD.
- [x] Reject a second command while the previous command is awaiting host synchronization.
- [x] Expose a host sync callback for after run_game_loop() returns.
- [ ] Connect submission to a real Angband frontend/game-loop host and verify turn processing.

## M3 — Snapshot bridge
- [x] Export a 21x21 player-centred snapshot.
- [x] Export known terrain, visibility and lighting.
- [x] Gate monster/object occupancy behind current visibility.
- [x] Export visibility-safe monster and object entity records.
- [x] Export rubble and shop terrain identifiers.
- [ ] Add complete terrain feature identifiers, including door states and visible traps.

## M4 — Event bridge
- [x] Subscribe to EVENT_PLAYERMOVED.
- [x] Emit authoritative player movement coordinates at the host sync boundary.
- [x] Refresh the snapshot after host synchronization, including blocked moves.
- [ ] Add combat, damage, death, object and terrain events at their authoritative resolution points.

## M5 — Diagnostic frontend
- [x] Add a CMake/CTest bridge test target and GitHub Actions workflow.
- [x] Test snapshot handoff and event-buffer growth.
- [ ] Bootstrap the pinned engine and compile the real adapter against its configured headers.
- [ ] Start Angband through a host frontend.
- [ ] Queue one ABFP movement command and call the host sync callback after run_game_loop().
- [ ] Compare before/after snapshots and movement event.
- [ ] Verify blocked movement, diagonal movement and monster interruption.

## M6 — Unreal renderer vertical slice
- [x] Create an Unreal Engine 5 C++ project and module scaffold.
- [x] Define Unreal snapshot structures matching the bridge protocol.
- [x] Render floor, wall and door blockout geometry from a snapshot.
- [x] Render persistent procedural 3D creature and item actors.
- [x] Add first-person mouse look, lantern light and interpolated discrete movement in a mock scene.
- [ ] Replace mock input with the live C bridge and authoritative snapshot callback.
- [ ] Add production materials, environment art, creature-specific models/rigs and animation.
- [ ] Add UI for health, status, inventory, combat messages and target inspection.

## Definition of success

The player presses Forward, Angband processes a normal command through its own game loop, and the renderer receives a fresh 21x21 view. The camera moves only after the host publishes the authoritative player position. Every monster, attack, object and turn remains governed by Angband.
