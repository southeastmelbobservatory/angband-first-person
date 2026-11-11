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
- [ ] Add safe monster/object identity and entity extraction.
- [ ] Add complete terrain feature identifiers.

## Movement proof
- [x] Capture authoritative player coordinates before movement.
- [x] Emit from/to coordinates on `EVENT_PLAYERMOVED`.
- [x] Refresh snapshot after movement.
- [ ] Verify blocked movement, diagonal movement, and monster interruption in a running build.
