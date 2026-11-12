# Angband API Findings — 4.2.6

Source inspected at the exact peeled release commit f3082213b73f3e463e3d0d60bff4b00462beae6 (tag 4.2.6).

## Command queue
The UI frontends enqueue commands with cmdq_push(). Directional walk is CMD_WALK; its direction argument is set with cmd_set_arg_direction(cmdq_peek(), "direction", dir). The main loop processes queued commands through cmdq_execute() in process_player().

The bridge only queues commands. The host's normal Angband game loop must call the normal command-processing path; do not independently execute the queue from a renderer tick. cmdq_push() returns an error code, with zero indicating success.

## Directions
The actual keypad constants are declared in cave.h: NW=7, N=8, NE=9, W=4, E=6, SW=1, S=2, SE=3.

## Events
EVENT_PLAYERMOVED is declared in game-event.h; handlers use event_add_handler() and event_remove_handler(). The bridge records the authoritative player grid before it queues a walk and publishes the actual grid on the movement event. Blocked or interrupted movement still needs runtime verification.

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
