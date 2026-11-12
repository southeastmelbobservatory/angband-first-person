# Unreal Engine 5 Client

The presentation client targets Unreal Engine 5. The renderer is deliberately separate from the Angband simulation: the engine decides movement, combat, AI, item state and turn order; Unreal displays the resulting snapshot and events.

## Current status

- Unreal C++ project/module scaffold.
- Snapshot and event structs matching the bridge protocol.
- Loopback TCP client with newline-delimited JSON decoding and lossless 64-bit entity IDs.
- Live movement command submission; the pawn waits for authoritative post-turn snapshots before accepting another move.
- Instanced floor, wall and door geometry with separate stone/bronze materials.
- Dungeon fog, ambient skylight and a warm player lantern.
- Persistent 3D entity actors keyed by bridge entity ID.
- In-world status readout showing authoritative depth, turn, HP, level and gold.
- Procedural articulated creature actor with idle motion (not a billboard).
- Procedural floating item actor.
- First-person camera, mouse look, warm lantern light, one-grid-step WASD controls and Space to wait.

At startup, the pawn attempts to connect to the local Angband host at 127.0.0.1:47652. If connected, movement commands are sent to Angband and the scene updates only from authoritative snapshots. If no host is available, it falls back to a clearly labelled local mock snapshot; mock movement is a renderer test only and must not be mistaken for Angband gameplay.

## Open the project

Open client/AngbandFirstPerson.uproject in Unreal Engine 5 and allow the C++ module to compile. W/A/S/D move one grid cell relative to the camera; Space waits one turn; mouse movement looks around.

## Next integration

The basic TCP command/state path is now wired, including authoritative player status. Remaining work is to compile and run the Unreal project, verify all eight movement directions and blocked moves, reconnect handling, event-driven animations, and gameplay lifecycle. Do not predict movement or locally move monsters in live mode.

The Unreal project has not yet been compiled or run in an Unreal Editor environment. It currently uses Unreal's built-in basic meshes as procedural placeholders. Production art, materials, creature-specific rigs, animation blueprints, audio, dungeon set dressing and collision are later art/engineering passes.
