# Real Integration Checklist

## Upstream
- [x] Pin official Angband 4.2.6 release commit.
- [x] Compile the adapter translation unit against the exact pinned Angband 4.2.6 headers in CI.
- [x] Link the bridge, adapter, host hook and loopback transport into the patched Angband CMake executable.
- [x] Build the patched Angband host executable in GitHub Actions.
- [ ] Confirm a clean local bootstrap/build on the target Linux host.
- [ ] Run the host interactively and verify save/load, birth, death, quit and restart paths.
- [ ] Record local build/test result.

## Frontend integration
- [x] Locate command queue API and movement event.
- [x] Implement command translation for movement and wait.
- [x] Register player-moved event handler.
- [x] Add explicit host sync callback after `run_game_loop()` returns.
- [x] Trace the pinned text frontend lifecycle and identify the supported command-hook seam (`cmd_get_hook`) plus required post-loop sync point in `play_game()`.
- [x] Add a revision-checked patch to `main.c`, `ui-game.c` and Angband's CMake build while preserving the normal `play_game()` lifecycle.
- [x] Implement bounded, thread-safe TCP command ingress with loopback default and opt-in trusted-LAN binding; the transport worker never calls Angband.
- [x] Consume one presentation command on the Angband game thread and submit it through Angband's normal command queue.
- [x] Publish the initial snapshot on client connection and publish refreshed snapshots/events after each completed game loop.
- [x] Retain queued events when snapshot delivery fails; clear them only after successful delivery.
- [x] Add CI jobs for standalone bridge tests and the complete pinned Angband host build.
- [x] Add a standalone loopback transport command parser regression test.
- [x] Add a minimal interactive protocol client for live native-host smoke testing.
- [x] Execute a live interactive host/client session and verify turn processing in CI.
- [ ] Compile the Unreal project and verify the live host/client connection.

## Snapshot
- [x] Export 21x21 player-centred terrain window.
- [x] Preserve unknown vs remembered vs currently visible distinctions.
- [x] Export lighting flag.
- [x] Prevent unseen monster/object occupancy leakage.
- [x] Export visible monster identity and hit points.
- [x] Export bounded Angband display names for visible monsters and known objects.
- [x] Export visible floor objects with unidentified-item privacy.
- [x] Distinguish rubble and shops from generic walls.
- [ ] Add complete terrain feature identifiers (door states, traps, mineral veins, webs, etc.).
- [x] Export core player statistics (HP, maximum HP, level and gold).
- [ ] Export status effects and equipment summary.

## Movement proof
- [x] Capture authoritative player coordinates before movement.
- [x] Emit movement only after the host sync boundary.
- [x] Clear pending command state for blocked and successful moves.
- [x] Refresh snapshot after host sync.
- [x] Derive visible monster movement and damage events from consecutive authoritative snapshots.
- [x] Derive authoritative player-damage events from consecutive snapshots.
- [x] Route stairs and autopickup through the original Angband command queue.
- [x] Exercise `AUTOPICKUP`, `GO_UP` and `GO_DOWN` no-op paths in the live host without blocking on text-UI message prompts.
- [x] Verify successful down/up stair traversal and an actual floor-item pickup event in the live Angband host (CI fixture uses real Angband terrain and object APIs).
- [x] Emit authoritative pickup events for floor objects removed by autopickup.
- [x] Verify blocked movement returns a snapshot without moving or consuming a turn in the live CI build.
- [x] Verify player bump-attacks use Angband combat, export monster HP damage and emit a death event when the monster is killed.
- [x] Verify visible monster movement events from Angband AI in a running host.
- [x] Verify player movement into a visible monster routes to Angband's attack action (monster interruption).
- [ ] Verify diagonal movement in a running build.
- [x] Verify client disconnect/reconnect and fresh initial-state delivery.
- [ ] Verify long-running play sessions and repeated reconnects.
- [x] Verify one successful movement command advances exactly one Angband turn; blocked movement advances none.

## Renderer
- [x] Create Unreal Engine 5 C++ project scaffold.
- [x] Render floor, wall and door blockout geometry from snapshots.
- [x] Render persistent 3D creature and item actors.
- [x] Add race-name and object-name driven procedural silhouette/material variants.
- [x] Add first-person look and interpolated grid movement in the mock scene.
- [ ] Connect Unreal input to the live host protocol.
- [x] Wire authoritative damage events to visible creature hit reactions.
- [x] Wire authoritative death events to visible creature death reactions.
- [ ] Verify authoritative turn timing in a running Angband host and Unreal client.
- [ ] Replace placeholder meshes with production art, materials and creature-specific rigs.
