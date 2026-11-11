# Angband API Findings — 4.2.6

Source inspected at the exact peeled release commit `f3082213b73f3e463e3d0d60bff4b00462beae6` (tag `4.2.6`).

## Command queue

The UI frontends enqueue commands with `cmdq_push()`. Directional walk is `CMD_WALK`; its `direction` argument is set with `cmd_set_arg_direction(cmdq_peek(), "direction", dir)`. The main loop processes queued commands through `cmdq_execute()` in `process_player()`.

The bridge only queues commands. The host's normal Angband game loop must call the normal command-processing path; do not independently execute the queue from a renderer tick. `cmdq_push()` returns an error code, with zero indicating success.

## Directions

The actual keypad constants are declared in `cave.h`: NW=7, N=8, NE=9, W=4, E=6, SW=1, S=2, SE=3.

## Events

`EVENT_PLAYERMOVED` is declared in `game-event.h`; handlers use `event_add_handler()` and `event_remove_handler()`.

## Snapshot source

- Current chunk: global `cave`
- Player: global `player`, position `player->grid`
- Chunk dimensions/depth/turn: `cave->width`, `cave->height`, `cave->depth`, `cave->turn`
- Bounds: `square_in_bounds(cave, grid)`
- Memory/visibility: `square_isknown()`, `square_isseen()`
- Lighting: `square_islit()`
- Terrain predicates: `square_isfloor()`, `square_isrock()`, `square_isdoor()`, `square_isupstairs()`, `square_isdownstairs()`
- Occupancy helpers: `square_monster()`, `square_object()`

Only known cells may be exported as terrain. A remembered but currently unseen cell must not be treated as currently visible. Do not expose unseen monster or object occupancy.

## Snapshot implementation

`abfp_upstream_refresh_snapshot()` exports a 21x21 window centred on the player into adapter-owned static cell storage and publishes it through the bridge. It fills terrain, visibility, lighting and visibility-gated monster/object occupancy. Entity records are intentionally empty in this milestone; stable entity IDs and identity disclosure rules are still to be designed.

The snapshot cell pointer remains valid until the next adapter snapshot refresh or adapter unload. The renderer should copy data it needs beyond that lifetime.
