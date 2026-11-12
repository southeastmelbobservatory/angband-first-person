# Real Integration Checklist

## Upstream
- [x] Pin official Angband 4.2.6 release commit.
- [ ] Bootstrap exact revision and build unchanged.
- [ ] Record local build/test result.

## Frontend integration
- [x] Locate command queue API and movement event.
- [x] Implement command translation for movement and wait.
- [x] Register player-moved event handler.
- [ ] Connect to a host that owns Angband's normal game loop.

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
- [x] Emit from/to coordinates on EVENT_PLAYERMOVED.
- [x] Refresh snapshot after movement.
- [ ] Verify blocked movement, diagonal movement, and monster interruption in a running build.

## Entity/rendering integration
- [x] Define protocol-level monster/object entity types and data IDs.
- [ ] Map Angband race and object-kind IDs to renderer assets.
- [ ] Add animated 3D creature rigs and object meshes.
- [ ] Verify entity lifetime/copy semantics in the host.
