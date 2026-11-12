# ABFP local command transport (initial)

The first host transport is deliberately small and loopback-only, for a local Unreal client and Angband host on the same machine.

- TCP listener binds to IPv4 loopback only (127.0.0.1), never a LAN interface.
- One client is served at a time and may reconnect after disconnecting.
- Each command is one ASCII line terminated by LF.
- Unknown commands and overlong lines are discarded.
- The queue is bounded at 64 commands; excess commands are dropped.
- The worker only parses and queues. The Angband game thread blocks in abfp_transport_wait_command() at the normal game input hook, then calls abfp_frontend_submit_command() itself.
- abfp_transport_stop() wakes the waiting game thread and closes the listener.

Commands: WAIT, MOVE_NORTH, MOVE_SOUTH, MOVE_WEST, MOVE_EAST, MOVE_NORTH_WEST, MOVE_NORTH_EAST, MOVE_SOUTH_WEST, MOVE_SOUTH_EAST.

This is command ingress only. Snapshot/event serialization and the pinned Angband cmd_get_hook/post-loop patch remain separate. Do not expose this unauthenticated, unencrypted listener to an untrusted network.


## State response

After the first client command, and after each completed Angband game loop, the
host sends one newline-delimited JSON object on the same TCP connection:

- type: "state"
- turn, depth, initial, player: [x, y]
- cells: arrays [x, y, terrain, visible, lit, occupied_monster, occupied_object]
- entities: arrays [id, x, y, type, kind_id, category_id, variant_id, hp, max_hp, quantity]
- events: arrays [type, turn, subject_id, target_id, from_x, from_y, to_x, to_y, value]

Coordinates, flags, enum values, depth, turn, and hit points are numeric; 64-bit entity/event IDs are decimal strings to avoid JSON floating-point precision loss. The initial boolean distinguishes the first snapshot from post-turn updates. The arrays preserve the
protocol enum values in protocol/game_state.h and protocol/events.h. The host
serializes and sends on the Angband game thread after the normal game loop; the
network worker never reads game state. Event records are cleared after each
publish attempt. The host waits for the TCP client to connect, sends the initial state before accepting the first command, then sends post-turn state after each normal Angband loop.

This response is currently implemented as host output; the Unreal-side socket
reader and JSON decoder are still to be connected.
