# Angband Engine Bridge

The bridge adapts the upstream Angband engine to the presentation protocol.

## Responsibilities

- submit player commands
- advance the authoritative simulation
- export snapshots
- emit ordered presentation events

Do not duplicate Angband rules here. The bridge is an adapter, not a replacement game engine.
