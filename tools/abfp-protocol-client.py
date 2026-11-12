#!/usr/bin/env python3
"""Minimal manual client for the native ABFP Angband host.

Use this while validating the real Angband bridge before Unreal is involved.
The host must already be running and waiting for a game-context command client.
"""

import argparse
import json
import socket
import sys

VALID = {
    "n": "MOVE_NORTH", "s": "MOVE_SOUTH", "w": "MOVE_WEST", "e": "MOVE_EAST",
    "nw": "MOVE_NORTH_WEST", "ne": "MOVE_NORTH_EAST",
    "sw": "MOVE_SOUTH_WEST", "se": "MOVE_SOUTH_EAST", ".": "WAIT",
}

def receive_line(sock, buffer):
    while b"\n" not in buffer:
        chunk = sock.recv(65536)
        if not chunk:
            raise ConnectionError("host disconnected")
        buffer += chunk
    line, _, buffer = buffer.partition(b"\n")
    return line, buffer

def describe(state):
    entities = state.get("entities", [])
    print(
        f"turn={state.get('turn')} depth={state.get('depth')} "
        f"player={state.get('player')} cells={len(state.get('cells', []))} "
        f"entities={len(entities)} events={len(state.get('events', []))}"
    )
    for event in state.get("events", []):
        print("  event:", event)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=47652)
    args = parser.parse_args()

    with socket.create_connection((args.host, args.port)) as sock:
        buffer = b""
        print("Connected. Waiting for authoritative initial snapshot...")
        line, buffer = receive_line(sock, buffer)
        state = json.loads(line)
        describe(state)
        print("Commands: n s e w ne nw se sw .  (q quits client)")

        for raw in sys.stdin:
            key = raw.strip().lower()
            if key in ("q", "quit", "exit"):
                return
            command = VALID.get(key)
            if not command:
                print("Unknown command")
                continue

            sock.sendall((command + "\n").encode("ascii"))
            try:
                line, buffer = receive_line(sock, buffer)
            except ConnectionError as exc:
                print(exc)
                return
            state = json.loads(line)
            describe(state)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
