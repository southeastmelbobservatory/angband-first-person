# Angband Host Contract

The bridge is an in-process adapter. It is not a network service and it does not own Angband startup, character creation, save loading, or the game loop. The presentation client must not call Angband internals directly.

## Thread and lifecycle

All bridge calls that touch Angband state must run on the same thread as Angband's game loop. Do not call them from an Unreal render tick or an arbitrary networking thread. If a separate process is used for Unreal, a future IPC host must marshal commands onto the Angband thread and copy snapshots/events out.

After Angband has initialized its modules and a live player/cave exists:

```c
if (!abfp_frontend_register()) {
    /* Handle bridge initialization failure. */
}
```

The host then owns the authoritative command boundary:

```c
while (!player->is_dead && player->upkeep->playing) {
    abfp_command_type command = host_wait_for_command();

    if (!abfp_frontend_submit_command(command)) {
        /* Reject/report the command; do not call run_game_loop for it. */
        continue;
    }

    run_game_loop();
    abfp_frontend_after_game_loop();

    const abfp_snapshot *snapshot = abfp_frontend_snapshot();
    host_publish_snapshot_and_events(snapshot);
}
```

The snippet describes the intended ownership contract, not a complete Angband executable. The actual host still needs to use Angband's supported initialization, birth/load, UI/event, save, death and shutdown lifecycle. `host_wait_for_command()` and `host_publish_snapshot_and_events()` are transport/UI functions to be implemented by the host; they are not supplied by this bridge.

## Rules

- Submit at most one command before each `run_game_loop()` call. The adapter rejects a second command while one is pending.
- Call `abfp_frontend_after_game_loop()` immediately after the normal game loop returns, including when the move was blocked.
- The host must not separately call `cmdq_execute()` or `process_player()`; that would bypass Angband's established loop.
- Snapshot arrays are adapter-owned and may be overwritten at the next synchronization. Copy them before retaining or publishing asynchronously.
- Read events using `abfp_bridge_event_count()` and `abfp_bridge_event_at()`, publish/copy them, then call `abfp_bridge_clear_events()`.
- Never infer unseen monsters, hidden objects, or unidentified object details from the renderer. The bridge intentionally filters entity occupancy by current visibility.
- Do not call the bridge concurrently with Angband state mutation. The current bridge has no cross-thread synchronization.
- On shutdown, call `abfp_bridge_shutdown()` before Angband tears down its event system and game state.

## Current gap

The frontend facade and pinned-engine adapter are present. A production host is still required to own Angband initialization and command acquisition and to provide IPC between a separate Unreal process and the Angband game thread. The current Unreal mock scene does not connect to this facade yet.
