# Unreal Engine 5 Client

The presentation client targets Unreal Engine 5. The renderer is deliberately separate from the Angband simulation: the engine decides movement, combat, AI, item state and turn order; Unreal displays the resulting snapshot and events.

## Current status

- Unreal C++ project/module scaffold.
- Snapshot and event structs matching the bridge protocol.
- TCP client with newline-delimited JSON decoding and lossless 64-bit entity IDs.
- Live movement command submission; the pawn waits for authoritative post-turn snapshots before accepting another move.
- Instanced floor, wall and door geometry with separate stone/bronze materials.
- Distinct 3D up/down staircases, rubble piles and shop entrance geometry driven by Angband terrain snapshots.
- Dungeon fog, ambient skylight and a warm player lantern.
- Persistent 3D entity actors keyed by bridge entity ID.
- Visible monsters interpolate between authoritative grid positions; race and object-kind names drive the procedural appearance, damage events trigger hit reactions, and death events play a short creature death animation.
- In-world status readout showing authoritative depth, turn, HP, level and gold; player damage events flash the lantern/status readout red.
- Creature death events play a short scale/fade/spin reaction before the procedural actor is destroyed.
- Procedural articulated creature actor with idle motion (not a billboard).
- Procedural floating item actor.
- First-person camera, mouse look, warm lantern light, one-grid-step WASD controls and Space to wait.

At startup, the pawn attempts to connect to the configured Angband host (default 127.0.0.1:47652). It keeps retrying if the host is not running yet, and reconnects after a dropped connection. If no host is available, it shows a clearly labelled local mock snapshot until a live host becomes available; mock movement is a renderer test only and must not be mistaken for Angband gameplay. The pawn's HostAddress and HostPort defaults can be overridden in a Blueprint child.

## Connecting across a trusted LAN

The native host binds to 127.0.0.1 by default. To run the Angband host on a different Linux PC, set `ABFP_BIND_ADDRESS` to that PC's LAN IPv4 address before launching it, for example:

```sh
ABFP_BIND_ADDRESS=192.168.0.120 bash tools/run-native-demo.sh
```

Then set the pawn Blueprint's HostAddress to `192.168.0.120` and HostPort to `47652`. Allow inbound TCP port 47652 in the host firewall. Binding to `0.0.0.0` is also supported but exposes the listener on every IPv4 interface. The protocol currently has no authentication or encryption: only expose it to a trusted private LAN, never directly to the internet.

## Build the Windows editor target

From the repository root in PowerShell, with Unreal Engine 5 installed:

```powershell
# Optional if the script cannot auto-detect your engine installation:
$env:UE_5_ROOT = "C:\Program Files\Epic Games\UE_5.6"

.\tools\build-unreal-client.ps1
```

The script builds the `AngbandFirstPersonEditor` Win64 Development target. You can also pass the engine installation directory explicitly:

```powershell
.\tools\build-unreal-client.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.6"
```

A successful build only confirms the C++ editor target compiles. Open `client/AngbandFirstPerson.uproject`, press Play, and connect it to the native host to verify the actual rendered game loop.

## Open the project

Open client/AngbandFirstPerson.uproject in Unreal Engine 5 and allow the C++ module to compile. W/A/S/D move one grid cell relative to the camera; Space waits one turn; mouse movement looks around.

## Next integration

The basic TCP command/state path is wired, including authoritative player status, combat damage and death reactions. The native host CI verifies a successful directional move, blocked movement, real Angband combat, death events and host-side reconnect; diagonal movement and monster interruption remain to be exercised. Remaining work is to compile and run the Unreal project, verify its live socket reconnect behaviour and event-driven animations in-editor, and exercise the complete gameplay lifecycle. Do not predict movement or locally move monsters in live mode.

The Unreal project has not yet been compiled or run in an Unreal Editor environment. It currently uses Unreal's built-in basic meshes as procedural placeholders. Production art, materials, creature-specific rigs, animation blueprints, audio, dungeon set dressing and collision are later art/engineering passes.
