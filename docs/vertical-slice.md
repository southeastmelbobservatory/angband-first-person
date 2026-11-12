# Vertical Slice 001 — The First Room

## Goal

Prove that the original simulation can drive a first-person 3D scene without changing Angband gameplay.

## Scene

- Stone floor, walls and doors built from the exported terrain snapshot
- First-person camera with free mouse look and warm lantern light
- Separate articulated 3D creature actor (not a billboard)
- Separate 3D object actor
- Turn/depth diagnostics

## Controls

- W/A/S/D: one discrete grid step relative to the camera
- Mouse: free visual look; does not consume a game turn
- Escape: menu (not yet wired)

The current Unreal scene uses a local mock snapshot to test presentation and camera movement. This is not live Angband gameplay. In live mode, each input must submit one command to Angband, wait for the host's normal game loop and end-of-loop sync, then interpolate the camera to the new authoritative player grid.

## Acceptance criteria

- Blocked cells remain blocked exactly as determined by Angband
- Monster movement originates from the engine
- Combat results originate from the engine
- Rendered positions match exported grid state
- A debug grid can validate the 3D scene
- The renderer can resynchronise from a complete snapshot
- Camera movement never predicts or overrides the authoritative player position
