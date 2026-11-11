# Architecture

## One authoritative simulation

The Angband engine owns turns, dungeon generation, terrain, line of sight, monsters, combat, spells, objects, inventory and save/load.

The 3D client owns meshes, animation, camera, lighting, particles, audio and UI presentation.

## Data flow

```
Input command -> engine adapter -> Angband simulation
                              |
                    snapshot + ordered events
                              |
                              v
                       3D presentation
```

Events describe what happened; the renderer never changes the result.

## Coordinate mapping

One logical Angband grid cell maps to one configurable 3D world cell:

`world_x = grid_x * CELL_SIZE`
`world_z = grid_y * CELL_SIZE`

## Rendering

The renderer interprets neighbouring cells to construct continuous dungeon geometry rather than placing isolated cubes for each tile.

## Determinism

The bridge should support deterministic replay from an engine version, seed and recorded command stream.
