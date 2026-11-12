# Unreal Engine 5 Client

The presentation client targets Unreal Engine 5. The renderer is deliberately separate from the Angband simulation: the engine decides movement, combat, AI, item state and turn order; Unreal displays the resulting snapshot and events.

## Current status

- Unreal C++ project/module scaffold.
- Snapshot structs matching the bridge protocol.
- Instanced floor, wall and door geometry with separate stone/bronze materials.
- Dungeon fog, ambient skylight and a warm player lantern.
- Persistent 3D entity actors keyed by bridge entity ID.
- Procedural articulated creature actor with idle motion (not a billboard).
- Procedural floating item actor.
- First-person camera, mouse look, warm lantern light and one-grid-step WASD controls.

The current pawn runs a clearly labelled local mock snapshot so the room can be explored before the live engine host exists. Its local movement is a renderer test only and must not be mistaken for Angband gameplay.

## Open the project

Open client/AngbandFirstPerson.uproject in Unreal Engine 5 and allow the C++ module to compile. W/A/S/D move one grid cell relative to the camera; mouse movement looks around.

## Next integration

Replace the pawn's TryDemoStep path with command submission through the C bridge. The pawn must wait for a new authoritative snapshot before recentering the world. Do not predict movement or locally move monsters in live mode.

The Unreal project has not yet been compiled or run in an Unreal Editor environment. It currently uses Unreal's built-in basic meshes as procedural placeholders. Production art, materials, creature-specific rigs, animation blueprints, audio, dungeon set dressing and collision are later art/engineering passes.
