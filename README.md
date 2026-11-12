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

- Official Angband 4.2.6 source revision pinned in the repository.
- Optional C bridge to translate movement/wait input into Angband commands.
- 21x21 player-centred dungeon snapshot with known terrain, visibility and lighting.
- Visibility-gated monster and floor-object records, including known game IDs and monster hit points.
- Protocol-level unit tests for snapshot handoff and event queue growth.

The upstream adapter is not yet proven against a locally built Angband executable. The host must run Angband's normal game loop and copy snapshot data before the next refresh.

## Build the bridge tests

Requires CMake 3.20+ and a C compiler:

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

This builds and tests the protocol/bridge layer without the Angband dependency.

See [docs/architecture.md](docs/architecture.md), [docs/bridge-roadmap.md](docs/bridge-roadmap.md), and [engine/bridge/INTEGRATION_CHECKLIST.md](engine/bridge/INTEGRATION_CHECKLIST.md).

## First playable milestone

Create a vertical slice where Angband produces a dungeon state, the bridge exports a visible area, and a 3D client renders it in first person while every movement command advances exactly one Angband turn.
