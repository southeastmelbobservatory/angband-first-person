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
