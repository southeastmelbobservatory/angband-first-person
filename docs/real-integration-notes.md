# Real Angband Integration Notes

The official Angband architecture already exposes two concepts that fit this project unusually well:

- a command queue for sending player commands to the game engine
- an event system for notifying frontends when game state changes

The first implementation should therefore integrate as a **new frontend**, not attempt to run a second game loop beside Angband.

## Recommended direction

```
ABFP frontend
    |
    +-- translates first-person input into Angband commands
    |
    +-- receives Angband frontend events
    |
    +-- queries authoritative dungeon/player state
    |
    +-- drives the 3D presentation
```

The first milestone queues one movement command, lets the normal Angband loop process it, then exports the resulting authoritative state.
