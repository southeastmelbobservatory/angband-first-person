# First Playable Demo

This is the first end-to-end milestone for the project:

> **The original Angband engine owns the game. The first-person client renders its authoritative state and sends one player action at a time.**

## What currently works in the code

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
- Walking into a monster uses Angband's normal movement/combat behaviour.
- Visible monster movement and HP loss are exported as events; the current creature placeholder plays a hit reaction.
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

The connection is intentionally loopback-only.

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

The source implementation is ready for the first real runtime verification, but this document does not claim that Unreal compilation/runtime validation has already happened. The next hard gate is:

1. successful native host build;
2. successful CTest run;
3. interactive Angband session;
4. real client connection;
5. verified movement, blocked movement, combat and reconnect;
6. compiled Unreal session consuming those snapshots.

Only after those checks pass should the milestone be called a fully verified playable demo.
