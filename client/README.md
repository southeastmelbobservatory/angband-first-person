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
- First-person camera, warm lantern light, discrete arrow-key tank controls and Space to wait.

At startup, the pawn attempts to connect to the configured Angband host (default 127.0.0.1:47652). It keeps retrying if the host is not running yet, and reconnects after a dropped connection. If no host is available, it shows a clearly labelled local mock snapshot until a live host becomes available; mock movement is a renderer test only and must not be mistaken for Angband gameplay. The pawn's HostAddress and HostPort defaults can be overridden in a Blueprint child.

## Connecting across a trusted LAN

The native host binds to 127.0.0.1 by default. To run the Angband host on a different Linux PC, set `ABFP_BIND_ADDRESS` to that PC's LAN IPv4 address before launching it, for example:

```sh
ABFP_BIND_ADDRESS=192.168.0.120 bash tools/run-native-demo.sh
```

Then set the pawn Blueprint's HostAddress to `192.168.0.120` and HostPort to `47652`. Allow inbound TCP port 47652 in the host firewall. Binding to `0.0.0.0` is also supported but exposes the listener on every IPv4 interface. The protocol currently has no authentication or encryption: only expose it to a trusted private LAN, never directly to the internet.

## Build and run on Windows (WSL host + Unreal Editor)

From the repository root in PowerShell, with Unreal Engine and WSL installed:

```powershell
# Builds the Unreal C++ module, launches the Angband host in WSL, and opens Unreal Editor:
.\tools\run-demo.ps1

# Optional flags:
# .\tools\run-demo.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.8"
# .\tools\run-demo.ps1 -SkipBuild
# .\tools\run-demo.ps1 -ProjectPath "C:\Users\you\angband-first-person\client 5.8\AngbandFirstPerson.uproject"
```

To compile only the Unreal C++ editor target without launching the host or editor:

```powershell
# Optional if the script cannot auto-detect your engine installation:
$env:UE_5_ROOT = "C:\Program Files\Epic Games\UE_5.8"

.\tools\build-unreal-client.ps1
```

The script builds the `AngbandFirstPersonEditor` Win64 Development target. You can also pass the engine installation directory explicitly:

```powershell
.\tools\build-unreal-client.ps1 -EngineRoot "C:\Program Files\Epic Games\UE_5.6"
```

A successful build only confirms the C++ editor target compiles. Open `client/AngbandFirstPerson.uproject`, press Play, and connect it to the native host to verify the actual rendered game loop.

## Configure the native host connection

The default endpoint is `127.0.0.1:47652`, for running Unreal and the Angband host on the same PC. Change `client/Config/DefaultGame.ini` when the host is on another machine:

```ini
[/Script/AngbandFirstPerson.ABFPFirstPersonPawn]
HostAddress=192.168.0.50
HostPort=47652
```

Replace the example address with the Linux host's LAN address. The native host binds to loopback by default. To allow a separate LAN client, start it with:

```bash
ABFP_BIND_ADDRESS=0.0.0.0 ./tools/run-native-demo.sh
```

Allow TCP port `47652` through the host firewall on the private LAN only. The bridge has no authentication or encryption; do not expose this listener to the internet.

## Open the project

Open `client/AngbandFirstPerson.uproject` in Unreal Engine and allow the C++ module to compile (or run `.\tools\build-unreal-client.ps1` first).

### Controls

- **Up / Down** — step one grid cell forward or backward (bump into monsters to attack, or closed doors to open)
- **Left / Right** — smoothly turn 45° left or right for 8-direction movement
- **F** — interact with the square ahead (`ALTER`: open/close doors, disarm visible traps, clear rubble, open chests)
- **E** — pick up items on the current square (`AUTOPICKUP`)
- **Space** — wait one authoritative turn (`WAIT`)
- **Page Up / Page Down** — ascend / descend staircases (`GO_UP` / `GO_DOWN`)
- **H** — toggle the on-screen controls reference

### HUD

`AABFPHUD` renders a 2D overlay directly from the authoritative snapshot:
- Bottom-left: connection status, HP/MP bars, depth, level, gold, and turn
- Bottom-right: scrolling Angband message log (`EVENT_MESSAGE`)
- Top-right: 21×21 roguelike minimap with facing arrow, monsters, items, doors, veins, traps, and stairs
- Center-top: look-ahead monster nameplate + HP bar, plus contextual interaction prompts

## Wiring high-fidelity meshes & assets (`ABFPVisualCatalog`)

The renderer works out of the box with procedural fallback geometry, and is wired for drop-in high-fidelity PBR meshes, skeletal rigs, Animation Blueprints, and audio via `UABFPVisualCatalog`:

1. In the Unreal Content Browser, create `Content/Data/` → right-click → **Miscellaneous → Data Asset** → choose **`ABFPVisualCatalog`** and name it `DA_DungeonVisualCatalog`.
2. Populate any combination of:
   - **DungeonTerrain / TownTerrain:** `FloorVariants`, `CeilingVariants`, `WallVariants`, `VeinWallVariants`, `WallPillarVariants`, `DoorFrame`, `DoorClosedPanel`, `DoorOpenPanel`, `DoorBrokenPanel`, `StairsUp`, `StairsDown`, `Rubble`, `ShopFacade`, `TrapMarker`, and `WallTorchProp`. Unassigned slots keep their procedural fallback.
   - **Creatures:** map Angband `KindId` (`ridx`) or `NameContains` (e.g. `"goblin"`, `"skeleton"`, `"mouse"`, `"orc"`) to a `USkeletalMesh` + `AnimClass`/`IdleAnim` + `HitMontage`/`DeathMontage`, a `StaticMeshFallback`, or a custom `AABFPCreatureActor` Blueprint subclass.
   - **Items:** map `KindId`, `CategoryId`/`VariantId` (`tval`/`sval`), or `NameContains` (e.g. `"potion"`, `"scroll"`, `"ration"`, `"sword"`) to a `UStaticMesh` with optional subtle point light and pickup sound.
   - **Audio:** assign `FootstepSound`, `AttackSwingSound`, `PlayerHitSound`, `DoorOpenSound`, `StairsSound`, and `PickupDefaultSound`.
3. Enable your catalog in `client/Config/DefaultGame.ini`:

```ini
[/Script/AngbandFirstPerson.ABFPFirstPersonPawn]
VisualCatalogAsset=/Game/Data/DA_DungeonVisualCatalog.DA_DungeonVisualCatalog
```

For a first-person lantern or weapon mesh, create a Blueprint child of `AABFPFirstPersonPawn` and attach your mesh to `ViewModelMount` (it automatically sways and swings on `ATTACK` events).
