# Engine Integration

The upstream dependency is pinned to the official Angband 4.2.6 release commit documented in PINNED_REVISION.md.

Run the bootstrap script from the repository root:

```sh
./tools/bootstrap-angband.sh
```

This checks out the exact upstream revision under engine/upstream/angband. The optional bridge adapter is compiled with ABFP_WITH_ANGBAND=ON and expects the configured Angband source tree at engine/upstream/angband/src by default.

The bridge is a static library, not a standalone Angband executable. The final host must link the configured Angband engine and provide its normal game-loop lifecycle. The host must not run a second simulation loop or call Angband command processing from a renderer tick.

The default CMake build and CTest suite intentionally exercise only the protocol/bridge layer. A successful default test run does not prove that the optional upstream adapter compiles or that a complete game launches.
