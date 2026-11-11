# Vertical Slice 001 — The First Room

## Goal

Prove that the original simulation can drive a first-person 3D scene without changing Angband gameplay.

## Scene

- stone floor
- walls and ceiling
- player
- one monster
- one object
- one light source

## Controls

- W/A/S/D: submit directional movement commands
- Mouse: free visual look
- Escape: menu

Movement remains turn-based. The camera smoothly animates after the engine accepts a command.

## Acceptance criteria

- blocked cells remain blocked exactly as determined by Angband
- monster movement originates from the engine
- combat results originate from the engine
- rendered positions match exported grid state
- a debug grid can validate the 3D scene
- the renderer can resynchronise from a complete snapshot
