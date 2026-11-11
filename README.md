# Angband: First Person

A visually rich, first-person 3D presentation layer wrapped around the classic Angband game engine.

## Core principle

**Angband remains the game.** The original engine remains authoritative for dungeon generation, turns, combat, monsters, objects, magic and rules. This project transforms that state into an immersive first-person 3D world.

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

## First milestone

Create a vertical slice where Angband produces a dungeon state, the bridge exports a visible area, and a 3D client renders it in first person while every movement command advances exactly one Angband turn.

See [docs/architecture.md](docs/architecture.md).
