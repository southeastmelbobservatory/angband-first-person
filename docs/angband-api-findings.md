# Angband API Findings — 4.2.6

Source inspected at the exact peeled release commit f3082213b73f3e463e3d0d60bff4b00462beae6 (tag 4.2.6).

## Command queue and host lifecycle

The UI frontends enqueue commands with cmdq_push(). Directional walk is CMD_WALK; its direction argument is set with cmd_set_arg_direction(cmdq_peek(), "direction", dir). Angband processes commands through process_player() inside run_game_loop().

The bridge only queues commands. The host owns Angband's normal game loop. After run_game_loop() returns to the host's input boundary, the host must call abfp_adapter_sync_after_game_loop(). This is the authoritative synchronization point: it emits the player movement event if a queued move actually changed the player grid, clears the pending-command gate (including blocked moves), and publishes a fresh snapshot. Do not independently execute the queue from a renderer tick.

The host should submit at most one bridge command before calling run_game_loop() and the sync callback. The adapter rejects another command while that command is pending. cmdq_push() returns an error code, with zero indicating success.

## Directions and events

The keypad constants are declared in cave.h: NW=7, N=8, NE=9, W=4, E=6, SW=1, S=2, SE=3.

EVENT_PLAYERMOVED is declared in game-event.h; handlers use event_add_handler() and event_remove_handler(). The event handler records that a player move occurred. The public movement event is emitted at the host synchronization boundary, not immediately inside the Angband event callback, so the renderer receives the final player position after Angband has reached its normal input boundary. Blocked movement consumes its normal game action but produces no movement event; the host sync still clears the command gate and refreshes the snapshot.

## Snapshot source

- Current chunk: global cave
- Player: global player, position player->grid
- Chunk dimensions/depth/turn: cave->width, cave->height, cave->depth, cave->turn
- Bounds: square_in_bounds(cave, grid)
- Memory/visibility: square_isknown(), square_isseen()
- Lighting: square_islit()
- Terrain: square_isfloor(), square_isrock(), square_isdoor(), square_isrubble(), square_isshop(), square_isupstairs(), square_isdownstairs()
- Occupancy: square_monster(), square_object()
- Monster identity/status: monster midx, race ridx, hp, maxhp
- Object identity/knowledge: object oidx, known, next; known kind metadata is read only from the known object.

Only known cells may be exported as terrain. Remembered but unseen cells are not currently visible. Do not expose unseen monster/object occupancy or object kind/category/variant for unidentified objects.

## Snapshot implementation

The adapter exports a 21x21 player-centred window into adapter-owned static cell and entity arrays. Entity records are emitted only for currently visible squares. Monster records carry per-level monster indices and Angband race IDs; object records carry floor object indices and expose kind/tval/sval only when the object has a known counterpart. Unidentified visible objects remain generic pickups.

The client maps Angband data IDs to its own 3D models, materials, rigs, and animation sets. These IDs are not renderer asset identifiers. Cell/entity pointers remain valid until the next snapshot refresh or adapter unload; the renderer must copy data it needs beyond that lifetime.

The fixed entity buffer is capped at 1024 records. Records beyond the cap are omitted; raise or replace the cap if testing demonstrates a need.
