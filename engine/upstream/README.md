# Upstream Angband

The authoritative upstream source is:

https://github.com/angband/angband

This project intentionally does not copy a floating snapshot of upstream into this repository yet.

## Recommended integration

Clone the upstream source as a git submodule:

```bash
git submodule add https://github.com/angband/angband.git engine/upstream/angband
git commit -m "Add Angband upstream engine"
```

The bridge should be developed as a small, reviewable patch layer around a pinned upstream revision.

## Why

- upstream history remains intact
- updates can be merged deliberately
- our bridge changes remain easy to audit
- Angband remains recognisably the authoritative engine

## Licensing

Before distributing builds, preserve Angband's licensing and attribution requirements. Do not assume upstream tiles, sounds, fonts, or other assets share the same licence as the C source.
