# Angband API Findings — Initial Bridge

Pinned inspection baseline: 42cd156ac9f95ddf4311e88dd7ad462292d6d1d6.

## Command queue

Angband exposes cmdq_push(CMD_WALK), cmdq_peek(), cmd_set_arg_direction(..., "direction", dir), and cmdq_execute(CTX_GAME).

For ABFP movement, queue CMD_WALK with the normal Angband numeric direction. Do not call do_cmd_walk() directly.

## Direction mapping

- SW = 1
- S = 2
- SE = 3
- W = 4
- E = 6
- NW = 7
- N = 8
- NE = 9

## Player movement event

Angband provides EVENT_PLAYERMOVED through game-event.h. Register a handler with event_add_handler(EVENT_PLAYERMOVED, handler, user).

## Map refresh

EVENT_MAP and EVENT_REFRESH are useful invalidation signals for rebuilding the local presentation snapshot.

## Authoritative state

The current player position is player->grid. The current dungeon is the global cave pointer. Terrain can be queried with square predicates such as square_isfloor(), square_isrock(), square_isdoor(), square_isupstairs(), square_isdownstairs(), and square_isknown().

## First implementation scope

1. eight-direction walking
2. wait/hold
3. EVENT_PLAYERMOVED
4. local terrain snapshot export

Monster and object extraction follows after movement is proven.
