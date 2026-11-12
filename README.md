# Angband: First Person

A visually rich, first-person 3D presentation layer wrapped around the classic Angband game engine.

## Core principle

**Angband remains the game.** The original engine remains authoritative for dungeon generation, turns, combat, monsters, objects, magic and rules. This project transforms that state into an immersive first-person 3D world.

The 3D client is intended to use fully modelled environments, animated creature rigs and physical-looking item meshes—not a first-person camera pointed at enlarged terminal glyphs. Visual effects and animation must remain presentation-only; Angband decides every outcome.

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
- Native Angband host command hook that preserves Angband's character creation, save/load, and normal turn loop.
- Loopback-only TCP command ingress with a bounded, thread-safe queue; commands are submitted only on Angband's game thread.
- Host publishes authoritative state and event snapshots as newline-delimited JSON after each turn.
- Unreal client has a TCP command sender and JSON snapshot/event decoder; live movement waits for authoritative state.
- CI applies the integration patch and builds/links the patched Angband host executable.
- 21x21 player-centred dungeon snapshot with known terrain, visibility and lighting.
- Visibility-gated monster and floor-object records, including known game IDs and monster hit points.
- Protocol-level unit tests for snapshot handoff and event queue growth.

The patched Angband host compiles and links in CI, and the Unreal code now has the matching TCP/JSON client path. Neither the Unreal project nor a live host/client session has yet been compiled and run together; end-to-end runtime verification remains outstanding.

## Build the Angband host (Linux)

Install CMake, a C compiler, pkg-config, and the ncurses development package (for Debian/Ubuntu: `sudo apt install build-essential cmake pkg-config libncurses-dev`). Then run:

```sh
./tools/build-angband-host.sh
```

The script checks out the pinned Angband 4.2.6 revision, applies the ABFP integration patch, and builds `build-angband/game/angband`. The host listens on `127.0.0.1:47652` for newline-delimited commands. It is command-ingress only at this stage; the Unreal client does not yet receive snapshots.

## Build the bridge tests

Requires CMake 3.20+ and a C compiler:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

This builds and tests the protocol/bridge layer without the Angband dependency.

The Unreal Engine 5 presentation project is in [client/AngbandFirstPerson.uproject](client/AngbandFirstPerson.uproject); see [client/README.md](client/README.md) before opening it. The Unreal scene currently uses mock data, not the live engine.

See [docs/architecture.md](docs/architecture.md), [docs/bridge-roadmap.md](docs/bridge-roadmap.md), and [engine/bridge/INTEGRATION_CHECKLIST.md](engine/bridge/INTEGRATION_CHECKLIST.md).

## First playable milestone

Create a vertical slice where Angband produces a dungeon state, the bridge exports a visible area, and a 3D client renders it in first person while every movement command advances exactly one Angband turn.
