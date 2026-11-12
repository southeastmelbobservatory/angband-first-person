# Angband: First Person

A visually rich, first-person 3D presentation layer wrapped around the classic Angband game engine.

## Core principle

**Angband remains the game.** The original engine remains authoritative for dungeon generation, turns, combat, monsters, objects, magic and rules. This project transforms that state into an immersive first-person 3D world.

The 3D client is intended to use fully modelled environments, animated creature rigs and physical-looking item meshes—not a first-person camera pointed at enlarged terminal glyphs. Visual effects and animation remain presentation-only; Angband decides every outcome.

## Architecture

```
Angband engine
     |
     v
Engine bridge / event API
     |
     v
3D presentation client
```

The renderer must never decide game outcomes.

## Current implementation

- Official Angband 4.2.6 source revision pinned, with a revision-checked frontend/CMake patch.
- Native Angband host command hook preserves Angband's character creation, save/load and normal turn loop.
- Loopback-only TCP command ingress with a bounded, thread-safe queue; the transport worker never calls Angband.
- Commands are consumed on Angband's game thread and translated into normal Angband command-queue entries.
- After `run_game_loop()` returns, the host synchronises the adapter and publishes an authoritative 21×21 state snapshot plus queued events as newline-delimited JSON.
- Initial state is published when a presentation client connects; later snapshots follow completed game-loop iterations.
- Snapshot publication now retains queued events if delivery fails, rather than silently discarding them.
- Unreal client has a TCP command sender and JSON snapshot/event decoder; live movement waits for authoritative state.
- CI builds the standalone bridge tests and the complete patched Angband executable.
- Visibility-gated terrain, monster and floor-object records; unidentified objects do not leak their true kind.

**Runtime status:** the patched native host is built in CI, but a live interactive Angband session and Unreal-to-host session have not yet been verified. The Unreal project remains a mock-data development target until that end-to-end test is completed.

## Build the Angband host (Linux)

Install CMake, a C compiler, pkg-config and ncurses development headers (Debian/Ubuntu):

```sh
sudo apt update
sudo apt install build-essential cmake pkg-config libncurses-dev
./tools/build-angband-host.sh
```

The script checks out the pinned Angband 4.2.6 revision, applies the ABFP integration patch and builds `build-angband/game/angband`.

The host binds only to `127.0.0.1:47652`. It accepts one newline-delimited command at a time from the presentation client. Supported commands are `MOVE_NORTH`, `MOVE_SOUTH`, `MOVE_EAST`, `MOVE_WEST`, the four diagonal equivalents, `WAIT`, `GO_UP`, `GO_DOWN` and `AUTOPICKUP`. Each state record also includes authoritative player HP, maximum HP, level and gold. The host sends newline-delimited JSON state records back over the same connection.

### Manual bridge smoke test

After building, start Angband normally from the `build-angband/game` directory and complete the normal text UI steps until the character has entered the dungeon. In a second terminal run:

```sh
python3 tools/abfp-protocol-client.py
```

The client waits for the authoritative initial snapshot, then accepts `n`, `s`, `e`, `w`, `ne`, `nw`, `se`, `sw` and `.` for wait. Each command prints the resulting authoritative turn, player position, visible entity count, player statistics and bridge events. This is the intended first live verification tool before Unreal is used.

Angband's normal text UI remains responsible for birth, stores and other non-game contexts. Once in the dungeon, the host waits for the presentation client to submit game commands.

## Build and test the bridge

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

This tests the protocol/bridge layer independently of Angband.

## Unreal presentation client

The Unreal Engine 5 project is in [client/AngbandFirstPerson.uproject](client/AngbandFirstPerson.uproject); see [client/README.md](client/README.md) before opening it. The scene still uses mock data and is not yet runtime-verified against a live native host.

See [docs/FIRST_PLAYABLE_DEMO.md](docs/FIRST_PLAYABLE_DEMO.md) for the end-to-end demo flow, plus [docs/architecture.md](docs/architecture.md), [docs/bridge-roadmap.md](docs/bridge-roadmap.md) and [engine/bridge/INTEGRATION_CHECKLIST.md](engine/bridge/INTEGRATION_CHECKLIST.md).

## First playable milestone

Angband produces a dungeon state, the bridge exports the visible area, and the 3D client renders it in first person. Every movement command advances Angband's own game loop exactly once; the camera and entities update only from authoritative snapshots.
