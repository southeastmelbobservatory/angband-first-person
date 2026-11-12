# Real Integration Checklist

## Upstream
- [x] Pin official Angband 4.2.6 release commit.
- [x] Compile the adapter translation unit against the exact pinned Angband 4.2.6 headers in CI.
- [ ] Link the bridge into a real Angband host and run the game.
- [ ] Bootstrap exact revision and build unchanged.
- [ ] Record local build/test result.

## Frontend integration
- [x] Locate command queue API and movement event.
- [x] Implement command translation for movement and wait.
- [x] Register player-moved event handler.
- [x] Add explicit host sync callback after run_game_loop returns.
- [ ] Connect callback to a host that owns Angband's normal game loop.

## Snapshot
- [x] Export 21x21 player-centred terrain window.
- [x] Preserve unknown vs remembered vs currently visible distinctions.
- [x] Export lighting flag.
- [x] Prevent unseen monster/object occupancy leakage.
- [x] Export visible monster identity and hit points.
- [x] Export visible floor objects with unidentified-item privacy.
- [x] Distinguish rubble and shops from generic walls.
- [ ] Add complete terrain feature identifiers (door states, traps, mineral veins, webs, etc.).

## Movement proof
- [x] Capture authoritative player coordinates before movement.
- [x] Emit movement only after the host sync boundary.
- [x] Clear pending command state for blocked and successful moves.
- [x] Refresh snapshot after host sync.
- [ ] Verify blocked movement, diagonal movement, and monster interruption in a running build.

## Renderer
- [x] Create Unreal Engine 5 C++ project scaffold.
- [x] Render floor, wall and door blockout geometry from snapshots.
- [x] Render persistent 3D creature and item actors.
- [x] Add first-person look and interpolated grid movement in the mock scene.
- [ ] Connect Unreal input to the live C bridge.
- [ ] Verify authoritative turn timing in a running Angband host.
- [ ] Replace placeholder meshes with production art, materials and creature-specific rigs.
