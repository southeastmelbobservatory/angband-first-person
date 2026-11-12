# First Playable Demo

This is the first end-to-end milestone for the project:

> **The original Angband engine owns the game. The first-person client renders its authoritative state and sends one player action at a time.**

## Verified native integration

The GitHub Actions native-host workflow now builds the pinned Angband executable and runs a real interactive PTY + TCP session. The live test has verified:

- Real Angband character creation and entry into the dungeon.
- Initial 21×21 authoritative state snapshot (441 cells).
- `WAIT` advances the Angband turn.
- A legal directional move changes the authoritative player coordinates.
- The corresponding player-movement event is exported with the state snapshot.
- A move into a known wall returns a snapshot without moving or consuming a turn.
- `AUTOPICKUP`, `GO_UP` and `GO_DOWN` commands are accepted and return to the client even when the player is not standing on a relevant item or staircase; the native host suppresses blocking text-UI `-more-` prompts in 3D mode.
- A real Angband down-stair transition and return up-stair transition (`depth 0 → 1 → 0`).
- An eligible floor ration is picked up by Angband when the player walks onto it, and the bridge exports the matching pickup event.
- Visible Angband monsters and objects are exported with their Angband display names and authoritative state.
- A visible Angband monster is exported with its live HP, and its attack produces a player-damage event.
- A player bump-attack reduces the monster's authoritative HP and killing it exports a death event.
- A disconnected presentation client can reconnect and receive a fresh initial snapshot without restarting the Angband character.

Latest verified run: [Native bridge and Angband host](https://github.com/southeastmelbobservatory/angband-first-person/actions/runs/43190959324).

## Implemented behaviour (not all yet runtime-verified)

- Normal Angband startup and character creation remain in the text UI.
- In dungeon game context, the ABFP host waits for a presentation client.
- The client receives a 21×21 authoritative visibility-safe dungeon snapshot.
- WASD movement sends one Angband movement command.
- Space sends one authoritative wait command.
- Page Up/Page Down use Angband's normal staircase commands when standing on the appropriate stairs.
- E invokes Angband's normal autopickup command for floor loot.
- Angband resolves the complete turn.
- The post-loop bridge synchronises state and sends the next snapshot.
- Player movement is interpolated visually only after the authoritative state changes.
- Visible monsters and objects are rendered as 3D actors.
- Creature race names and object-kind names drive procedural silhouettes, proportions and material palettes.
- Walking into a monster uses Angband's normal movement/combat behaviour.
- Visible monster movement and HP loss are exported as events; creature hit reactions use the authoritative damage events.
- HP, maximum HP, character level and gold are displayed from the authoritative snapshot.

## Native host

On Linux:

```sh
bash tools/run-native-demo.sh
```

Complete the ordinary Angband UI flow to create or load a character. When the game reaches its first dungeon command boundary, it waits for the presentation client on:

```
127.0.0.1:47652
```

The host binds to loopback by default. For a separate Unreal client on a trusted LAN, set `ABFP_BIND_ADDRESS` on the Linux host and configure the pawn Blueprint's HostAddress; see [client connection instructions](../client/README.md#connecting-across-a-trusted-lan). The protocol has no authentication or encryption, so do not expose it to the internet.

## Unreal client

Open:

```
client/AngbandFirstPerson.uproject
```

Run Play in the Editor after the C++ module has compiled.

Controls:

- **W/A/S/D** — move one grid square relative to the camera
- **Space** — wait one Angband turn
- **Page Up/Page Down** — use stairs when standing on them
- **E** — invoke autopickup for items on the current square
- **Mouse** — look around

When connected, the in-world status readout says **ANGBAND LIVE**. The player position is not predicted: each action waits for the next authoritative Angband snapshot.

If no native host is listening, the client starts its deliberately labelled renderer-only mock scene instead.

## Manual protocol fallback

For native bridge troubleshooting without Unreal:

```sh
python3 tools/abfp-protocol-client.py
```

This client receives the same authoritative snapshots and can submit movement and wait commands.

## Remaining verification gate

Native host run #205 verifies the complete pinned Angband build, real character creation, authoritative snapshots, wait and movement, blocked movement, stair descent/ascent, floor-item pickup, visible monster state, monster-to-player damage, player-initiated attack damage, monster death events and host-side client reconnect. Remaining gates are:

1. verify visible monster movement events, diagonal movement and monster interruption in the live host;
2. verify longer play sessions and repeated reconnects;
3. clean local bootstrap/build and interactive save/load/death/quit/restart paths on the target Linux host;
4. compile the Unreal project in Unreal Engine 5 and connect its live input/rendering path to the native host;
5. polish collision, production art, animation rigs, materials, audio and dungeon set dressing.

The project has a verified native gameplay bridge, but it is not yet a fully verified 3D playable demo until the Unreal client is compiled and tested against it.
