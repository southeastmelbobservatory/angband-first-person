#!/usr/bin/env python3
"""Exercise a real Angband birth flow and one ABFP command/state round trip."""

import fcntl
import json
import os
import pty
import re
import select
import signal
import socket
import struct
import subprocess
import sys
import termios
import tempfile
import time


PORT = 47652


def terminal_text(output):
    # Angband emits colour and cursor-control sequences between words.
    text = bytes(output).decode(errors="replace")
    text = re.sub(r"\x1b(?:\[[0-?]*[ -/]*[@-~]|\][^\x07]*(?:\x07|\x1b\\)|[()][A-Z0-9])", "", text)
    return text.lower()


def wait_for_terminal_text(master, output, needle, timeout, pid):
    needle = needle.decode().lower()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        waited, status = os.waitpid(pid, os.WNOHANG)
        if waited:
            raise RuntimeError(
                "Angband exited during character creation:\n"
                + bytes(output).decode(errors="replace")[-5000:]
            )
        readable, _, _ = select.select([master], [], [], 0.1)
        if readable:
            try:
                chunk = os.read(master, 8192)
                if chunk:
                    output.extend(chunk)
            except OSError:
                pass
        if needle in terminal_text(output):
            return
    raise RuntimeError(
        f"Timed out waiting for terminal text {needle!r}:\n"
        + bytes(output).decode(errors="replace")[-5000:]
    )


def receive_state(sock, buffer, timeout=30, master=None, output=None):
    deadline = time.monotonic() + timeout
    while b"\n" not in buffer:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError("Timed out waiting for an Angband state line")
        readers = [sock]
        if master is not None:
            readers.append(master)
        readable, _, _ = select.select(readers, [], [], min(remaining, 0.1))
        if master is not None and master in readable:
            try:
                chunk = os.read(master, 8192)
                if chunk and output is not None:
                    output.extend(chunk)
            except OSError:
                pass
        if sock in readable:
            chunk = sock.recv(65536)
            if not chunk:
                raise ConnectionError("Angband host disconnected before sending state")
            buffer += chunk
    line, _, buffer = buffer.partition(b"\n")
    return json.loads(line), buffer


def main():
    binary = os.path.abspath(sys.argv[1])
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    upstream_dir = os.path.join(repo_root, "engine", "upstream", "angband")
    if not os.path.isfile(os.path.join(upstream_dir, "lib", "screens", "news.txt")):
        raise RuntimeError(f"Angband data directory is missing: {upstream_dir}/lib")

    with tempfile.TemporaryDirectory(prefix="abfp-live-roundtrip-") as home:
        user_dir = os.path.join(home, "user")
        save_dir = os.path.join(home, "save")
        os.makedirs(user_dir)
        os.makedirs(save_dir)

        pid, master = pty.fork()
        if pid == 0:
            os.chdir(upstream_dir)
            env = os.environ.copy()
            env.update({
                "HOME": home,
                "TERM": "xterm-256color",
                "LANG": "C.UTF-8",
                "LC_ALL": "C.UTF-8",
                "ABFP_TEST_FORCE_STAIRS": "1",
                "ABFP_TEST_FLOOR_ITEM": "1",
            })
            os.execve(binary, [
                binary, "-n", "-uABFPRuntime",
                f"-duser={user_dir}", f"-dsave={save_dir}",
            ], env)

        fcntl.ioctl(master, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 100, 0, 0))
        os.set_blocking(master, False)
        output = bytearray()
        client = None
        try:
            # The listener is installed before the normal Angband startup pause.
            deadline = time.monotonic() + 20
            while time.monotonic() < deadline:
                waited, status = os.waitpid(pid, os.WNOHANG)
                if waited:
                    raise RuntimeError(
                        "Angband exited before opening its listener:\n"
                        + bytes(output).decode(errors="replace")[-5000:]
                    )
                try:
                    client = socket.create_connection(("127.0.0.1", PORT), timeout=0.5)
                    break
                except OSError:
                    readable, _, _ = select.select([master], [], [], 0.1)
                    if readable:
                        try:
                            output.extend(os.read(master, 8192))
                        except OSError:
                            pass
            if client is None:
                raise RuntimeError("Timed out waiting for the ABFP TCP listener")

            # Dismiss Angband's startup pause, then let its ordinary birth UI run.
            time.sleep(0.3)
            os.write(master, b" ")
            wait_for_terminal_text(
                master, output, b"finish the character with random selections", 20, pid
            )
            os.write(master, b"@")
            wait_for_terminal_text(master, output, b"any other key to continue", 20, pid)
            # Wait for the terminal refresh to finish before answering the
            # final birth prompt; otherwise a PTY write can race the UI read.
            time.sleep(0.25)
            os.write(master, b"x")

            # The host publishes its first real dungeon snapshot at CTX_GAME.
            buffer = b""
            try:
                initial, buffer = receive_state(client, buffer, master=master, output=output)
            except (TimeoutError, ConnectionError) as error:
                # Drain any output produced after the confirmation key.
                drain_deadline = time.monotonic() + 1
                while time.monotonic() < drain_deadline:
                    readable, _, _ = select.select([master], [], [], 0.1)
                    if readable:
                        try:
                            output.extend(os.read(master, 8192))
                        except OSError:
                            break
                raise RuntimeError(
                    "No initial state after character confirmation. Captured terminal output:\n"
                    + terminal_text(output)[-5000:]
                ) from error
            if initial.get("type") != "state" or not initial.get("initial"):
                raise AssertionError(f"Expected initial state snapshot, got: {initial!r}")
            if not initial.get("cells") or len(initial["cells"]) < 100:
                raise AssertionError("Initial snapshot did not contain the dungeon view")
            if not isinstance(initial.get("player"), list) or len(initial["player"]) != 2:
                raise AssertionError(f"Invalid player position: {initial.get('player')!r}")

            client.sendall(b"WAIT\n")
            updated, buffer = receive_state(client, buffer, master=master, output=output)
            if updated.get("type") != "state":
                raise AssertionError(f"Expected updated state, got: {updated!r}")
            if updated.get("turn", 0) <= initial.get("turn", -1):
                raise AssertionError(
                    f"WAIT did not advance the authoritative turn: "
                    f"{initial.get('turn')} -> {updated.get('turn')}"
                )

            # Prove that a directional command changes the authoritative player
            # position, not merely that the host can advance a turn.
            adjacent_moves = [
                ("MOVE_NORTH", 0, -1),
                ("MOVE_EAST", 1, 0),
                ("MOVE_SOUTH", 0, 1),
                ("MOVE_WEST", -1, 0),
                ("MOVE_NORTH_EAST", 1, -1),
                ("MOVE_SOUTH_EAST", 1, 1),
                ("MOVE_SOUTH_WEST", -1, 1),
                ("MOVE_NORTH_WEST", -1, -1),
            ]
            cell_by_position = {
                (cell[0], cell[1]): cell for cell in updated.get("cells", [])
            }
            movement = None
            for command, dx, dy in adjacent_moves:
                target = (updated["player"][0] + dx, updated["player"][1] + dy)
                cell = cell_by_position.get(target)
                # Terrain 1 is known floor. Avoid monsters; floor objects do
                # not prevent ordinary Angband movement.
                if cell and cell[2] == 1 and cell[3] and not cell[5]:
                    movement = (command, target)
                    break
            if movement is None:
                raise AssertionError(
                    "No visible, unoccupied floor square adjacent to the "
                    f"player for movement verification: {updated['player']}"
                )

            move_command, expected_position = movement
            client.sendall((move_command + "\n").encode("ascii"))
            try:
                moved, buffer = receive_state(client, buffer, master=master, output=output)
            except TimeoutError as error:
                # Preserve terminal output; it can reveal whether Angband
                # rejected the command or is waiting for additional input.
                readable, _, _ = select.select([master], [], [], 0.2)
                if readable:
                    try:
                        output.extend(os.read(master, 8192))
                    except OSError:
                        pass
                raise AssertionError(
                    f"No state returned after {move_command} toward "
                    f"{expected_position}. Angband terminal: "
                    f"{terminal_text(output)[-2500:]}"
                ) from error
            if moved.get("type") != "state":
                raise AssertionError(f"Expected movement state, got: {moved!r}")
            if moved.get("turn", 0) <= updated.get("turn", -1):
                raise AssertionError(
                    f"{move_command} did not advance the authoritative turn: "
                    f"{updated.get('turn')} -> {moved.get('turn')}"
                )
            if tuple(moved.get("player", [])) != expected_position:
                raise AssertionError(
                    f"{move_command} did not move to {expected_position}: "
                    f"got {moved.get('player')}"
                )

            # The host should export the authoritative player-moved event
            # alongside the state snapshot, not merely update coordinates.
            movement_events = [
                event for event in moved.get("events", [])
                if len(event) >= 9 and event[0] == 1 and event[2] == "1"
            ]
            if not any(
                event[4] == updated["player"][0]
                and event[5] == updated["player"][1]
                and event[6] == expected_position[0]
                and event[7] == expected_position[1]
                for event in movement_events
            ):
                raise AssertionError(
                    "Movement snapshot did not include the matching "
                    f"authoritative player-moved event: {moved.get('events')!r}"
                )

            # Bumping a known wall is a no-energy Angband action. It must
            # return to the host input boundary without moving or consuming
            # a turn, rather than leaving the client waiting indefinitely.
            wall_moves = [
                ("MOVE_NORTH", 0, -1), ("MOVE_EAST", 1, 0),
                ("MOVE_SOUTH", 0, 1), ("MOVE_WEST", -1, 0),
                ("MOVE_NORTH_EAST", 1, -1), ("MOVE_SOUTH_EAST", 1, 1),
                ("MOVE_SOUTH_WEST", -1, 1), ("MOVE_NORTH_WEST", -1, -1),
            ]
            moved_cells = {
                (cell[0], cell[1]): cell for cell in moved.get("cells", [])
            }
            blocked = None
            for command, dx, dy in wall_moves:
                target = (moved["player"][0] + dx, moved["player"][1] + dy)
                cell = moved_cells.get(target)
                if cell and cell[2] == 2 and cell[3] and not cell[5]:
                    blocked = (command, target)
                    break
            if blocked is None:
                raise AssertionError(
                    "No visible adjacent wall square available to verify "
                    f"blocked movement at {moved['player']}"
                )

            blocked_command, blocked_target = blocked
            client.sendall((blocked_command + "\n").encode("ascii"))
            blocked_state, buffer = receive_state(
                client, buffer, master=master, output=output
            )
            if tuple(blocked_state.get("player", [])) != expected_position:
                raise AssertionError(
                    f"{blocked_command} into wall {blocked_target} unexpectedly "
                    f"changed player position: {blocked_state.get('player')}"
                )
            if blocked_state.get("turn") != moved.get("turn"):
                raise AssertionError(
                    f"Blocked movement should not consume a turn: "
                    f"{moved.get('turn')} -> {blocked_state.get('turn')}"
                )

            # Exercise the engine's own autopickup command. If an object
            # actually disappears from the player's square, require the
            # matching pickup event; an empty square is still a valid no-op.
            player_xy = tuple(blocked_state["player"])
            objects_before = {
                entity[0] for entity in blocked_state.get("entities", [])
                if len(entity) >= 10 and entity[3] == 2
                and (entity[1], entity[2]) == player_xy
            }
            client.sendall(b"AUTOPICKUP\n")
            pickup_state, buffer = receive_state(
                client, buffer, master=master, output=output
            )
            if tuple(pickup_state.get("player", [])) != player_xy:
                raise AssertionError("AUTOPICKUP unexpectedly moved the player")
            objects_after = {
                entity[0] for entity in pickup_state.get("entities", [])
                if len(entity) >= 10 and entity[3] == 2
                and (entity[1], entity[2]) == player_xy
            }
            removed_objects = objects_before - objects_after
            pickup_events = {
                event[3] for event in pickup_state.get("events", [])
                if len(event) >= 9 and event[0] == 5 and event[2] == "1"
            }
            if not removed_objects.issubset(pickup_events):
                raise AssertionError(
                    "AUTOPICKUP removed floor objects without matching pickup events: "
                    f"removed={removed_objects}, events={pickup_events}"
                )

            # The test host places a real Angband down-stair on the
            # player's square immediately before this command is submitted.
            # Only the fixture is synthetic; Angband performs the transition.
            starting_depth = pickup_state.get("depth")
            client.sendall(b"GO_DOWN\n")
            try:
                stair_state, buffer = receive_state(
                    client, buffer, timeout=15, master=master, output=output
                )
            except TimeoutError as error:
                readable, _, _ = select.select([master], [], [], 0.2)
                if readable:
                    try:
                        output.extend(os.read(master, 8192))
                    except OSError:
                        pass
                raise AssertionError(
                    "No state returned after forced GO_DOWN. "
                    f"Angband terminal: {terminal_text(output)[-3500:]}"
                ) from error
            if stair_state.get("depth") != starting_depth + 1:
                raise AssertionError(
                    f"GO_DOWN did not descend one level: "
                    f"{starting_depth} -> {stair_state.get('depth')}"
                )

            depth_after_down = stair_state.get("depth")
            dungeon_player_xy = tuple(stair_state.get("player", []))
            movement_by_delta = {
                (0, -1): "MOVE_NORTH", (1, 0): "MOVE_EAST",
                (0, 1): "MOVE_SOUTH", (-1, 0): "MOVE_WEST",
                (1, -1): "MOVE_NORTH_EAST", (1, 1): "MOVE_SOUTH_EAST",
                (-1, 1): "MOVE_SOUTH_WEST", (-1, -1): "MOVE_NORTH_WEST",
            }
            seeded_item = None
            for entity in stair_state.get("entities", []):
                if len(entity) < 10 or entity[3] != 2:
                    continue
                delta = (entity[1] - dungeon_player_xy[0],
                    entity[2] - dungeon_player_xy[1])
                if delta in movement_by_delta:
                    seeded_item = (entity[0], (entity[1], entity[2]), delta)
                    break
            if seeded_item is None:
                raise AssertionError(
                    "The test host did not expose its seeded ration on an "
                    f"adjacent floor tile: {stair_state.get('entities')!r}"
                )

            seeded_item_id, item_xy, item_delta = seeded_item
            client.sendall((movement_by_delta[item_delta] + "\n").encode("ascii"))
            item_state, buffer = receive_state(
                client, buffer, timeout=15, master=master, output=output
            )
            if tuple(item_state.get("player", [])) != item_xy:
                raise AssertionError(
                    f"Player did not move onto seeded item at {item_xy}: "
                    f"{item_state.get('player')}"
                )

            dungeon_floor_items_after = {
                entity[0] for entity in item_state.get("entities", [])
                if len(entity) >= 10 and entity[3] == 2
                and (entity[1], entity[2]) == item_xy
            }
            if seeded_item_id in dungeon_floor_items_after:
                raise AssertionError(
                    f"Angband did not auto-pick up ration {seeded_item_id} "
                    f"when entering {item_xy}"
                )

            dungeon_pickup_events = {
                event[3] for event in item_state.get("events", [])
                if len(event) >= 9 and event[0] == 5 and event[2] == "1"
            }
            if seeded_item_id not in dungeon_pickup_events:
                raise AssertionError(
                    "Automatic pickup removed the ration without emitting "
                    f"the matching event: item={seeded_item_id}, "
                    f"events={dungeon_pickup_events}"
                )

            return_delta = (-item_delta[0], -item_delta[1])
            client.sendall(
                (movement_by_delta[return_delta] + "\n").encode("ascii")
            )
            return_state, buffer = receive_state(
                client, buffer, timeout=15, master=master, output=output
            )
            if tuple(return_state.get("player", [])) != dungeon_player_xy:
                raise AssertionError(
                    f"Player did not return to the up-stair tile {dungeon_player_xy}: "
                    f"{return_state.get('player')}"
                )

            client.sendall(b"GO_UP\n")
            up_state, buffer = receive_state(
                client, buffer, timeout=15, master=master, output=output
            )
            if up_state.get("depth") != starting_depth:
                raise AssertionError(
                    f"GO_UP did not return to starting depth {starting_depth}: "
                    f"got {up_state.get('depth')} after descending to {depth_after_down}"
                )

            dungeon_monsters = [
                entity for entity in stair_state.get("entities", [])
                if len(entity) >= 10 and entity[3] == 1
            ]
            dungeon_monster_count = len(dungeon_monsters)
            dungeon_object_count = sum(
                1 for entity in stair_state.get("entities", [])
                if len(entity) >= 10 and entity[3] == 2
            )

            # The entity exporter must include actual visible Angband
            # monsters, not just floor geometry. Their HP fields are the
            # authoritative values the renderer will use for hit reactions.
            if not dungeon_monsters:
                raise AssertionError(
                    "Generated depth exposed no visible monster entities; "
                    "the native snapshot cannot yet drive creature rendering"
                )
            invalid_monsters = [
                entity for entity in dungeon_monsters
                if entity[8] <= 0 or entity[9] < entity[8]
            ]
            if invalid_monsters:
                raise AssertionError(
                    "Monster entities have invalid authoritative HP values: "
                    f"{invalid_monsters!r}"
                )

            print(
                "Live Angband round trip passed: "
                f"initial turn {initial['turn']}, wait turn {updated['turn']}, "
                f"movement turn {moved['turn']}, blocked move unchanged; "
                f"{len(initial['cells'])} snapshot cells; "
                f"player {initial['player']} -> {updated['player']} -> "
                f"{moved['player']} via {move_command}; "
                f"movement event exported; "
                f"town AUTOPICKUP no-op accepted; movement-triggered pickup event verified; "
                f"stairs depth {starting_depth}->{depth_after_down}->{up_state['depth']}; "
                f"new level visible monsters={dungeon_monster_count}, "
                f"objects={dungeon_object_count}."
            )
        finally:
            if client is not None:
                client.close()
            try:
                os.kill(pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            deadline = time.monotonic() + 3
            while time.monotonic() < deadline:
                try:
                    waited, _ = os.waitpid(pid, os.WNOHANG)
                except ChildProcessError:
                    # wait_for_terminal_text() may already have reaped it.
                    waited = pid
                if waited:
                    break
                time.sleep(0.05)
            else:
                try:
                    os.kill(pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                try:
                    os.waitpid(pid, 0)
                except ChildProcessError:
                    pass
            os.close(master)


if __name__ == "__main__":
    main()
