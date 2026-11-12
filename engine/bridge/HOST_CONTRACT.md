# Angband Host Contract

The bridge is an in-process adapter. It does not own Angband startup, character creation, save loading, the outer player-command loop, or shutdown. The presentation client must not call Angband internals directly.

## Pinned Angband 4.2.6 lifecycle

The supported text frontend in the pinned revision follows this sequence:

1. `main()` initializes the display, calls `init_angband()` and `textui_init()`.
2. `play_game()` calls the private `start_game()`, which loads a save or runs character birth, enters the world, and generates the initial level.
3. For each player input boundary, `play_game()` calls the private `pre_turn_refresh()`, then `cmd_get_hook(CTX_GAME)`, then `run_game_loop()`.
4. On death or quit, `play_game()` calls `close_game(true)`, which owns save/death handling and leave-world/leave-game events.
5. The outer frontend then cleans up the UI and Angband.

This matters: calling `run_game_loop()` alone does not start a game or provide the full frontend lifecycle. A custom host must preserve the outer lifecycle (including birth/load, refresh, save, death, and shutdown), not reimplement those pieces casually.

## Intended integration seam

The lowest-risk first host should retain Angband's existing `play_game()` lifecycle and replace only its command source:

- Keep `start_game()`, `pre_turn_refresh()`, `close_game()`, and all save/death handling inside Angband's existing `play_game()`.
- Replace `cmd_get_hook` with an ABFP command hook. The hook runs on the Angband game thread, waits for one validated command from the transport, and submits it through `abfp_frontend_submit_command()`.
- Add a post-`run_game_loop()` synchronization call in the pinned `ui-game.c` loop. This is the correct point to call `abfp_frontend_after_game_loop()`: it runs after Angband has processed the command and all resulting monster/world activity, including blocked moves.
- Initialize the bridge lazily on the first game-command hook, after `start_game()` has created a live player and cave. Do not initialize it during application startup or character birth.
- Copy the snapshot and events before publishing them to the network thread. Never let the network thread read Angband globals or adapter-owned snapshot buffers.

The command hook is synchronous from Angband's perspective. A transport worker may receive TCP data and place a validated command in a thread-safe queue, but only the game-thread hook may call the bridge or Angband APIs.

## Why the patch is required

In Angband 4.2.6, `start_game()`, `pre_turn_refresh()`, `select_savefile()`, and `close_game()` are private functions in `ui-game.c`. There is no public API that lets an external executable safely perform the complete start/load/birth/play/close lifecycle. The practical integration is therefore a small, pinned patch to the existing frontend loop, not a second independently-written game loop.

The patch should be version-checked against commit `f3082213b73f3e463e3d0d60bff4b00462beae6e` and applied by the bootstrap/build process. It must not silently apply to a different Angband revision.

## Bridge API and thread rules

After a live player/cave exists:

```c
if (!abfp_frontend_register()) {
    /* Handle bridge initialization failure. */
}
```

At each command boundary, on the Angband game thread:

```c
/* The ABFP cmd_get_hook obtains exactly one command from its transport. */
if (!abfp_frontend_submit_command(command)) {
    /* Reject/report the command; do not run the game loop for it. */
}

/* Angband's existing play_game() then calls run_game_loop(). */
run_game_loop();

/* Pinned frontend patch: synchronize after the normal loop returns. */
abfp_frontend_after_game_loop();
```

The snippet illustrates the seam only; it is not a replacement for `play_game()`.

Rules:

- Submit at most one command before each `run_game_loop()` call. The adapter rejects a second command while one is pending.
- Call `abfp_frontend_after_game_loop()` immediately after the normal game loop returns, including when the move was blocked.
- Do not separately call `cmdq_execute()` or `process_player()`; that bypasses Angband's established loop.
- Snapshot arrays are adapter-owned and may be overwritten at the next synchronization. Copy them before retaining or publishing asynchronously.
- Read events using `abfp_bridge_event_count()` and `abfp_bridge_event_at()`, publish/copy them, then call `abfp_bridge_clear_events()`.
- Never infer unseen monsters, hidden objects, or unidentified object details from the renderer. The bridge intentionally filters entity occupancy by current visibility.
- Do not call the bridge concurrently with Angband state mutation. The current bridge has no cross-thread synchronization.
- On shutdown, call `abfp_bridge_shutdown()` before Angband tears down its event system and game state.

## Current gap

The frontend facade and pinned-engine adapter compile against the exact Angband 4.2.6 headers. The lifecycle patch, command transport, native host executable, and live Unreal connection are not yet implemented. The current Unreal mock scene does not connect to this facade.
