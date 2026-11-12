#!/usr/bin/env python3
"""Exercise a real Angband birth flow and one ABFP command/state round trip."""

import fcntl
import json
import os
import pty
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


def wait_for_terminal_text(master, output, needle, timeout, pid):
    needle = needle.lower()
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
        if needle in bytes(output).lower():
            return
    raise RuntimeError(
        f"Timed out waiting for terminal text {needle!r}:\n"
        + bytes(output).decode(errors="replace")[-5000:]
    )


def receive_state(sock, buffer, timeout=30):
    sock.settimeout(timeout)
    while b"\n" not in buffer:
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
            wait_for_terminal_text(master, output, b"step back", 20, pid)
            os.write(master, b"x")

            # The host publishes its first real dungeon snapshot at CTX_GAME.
            buffer = b""
            initial, buffer = receive_state(client, buffer)
            if initial.get("type") != "state" or not initial.get("initial"):
                raise AssertionError(f"Expected initial state snapshot, got: {initial!r}")
            if not initial.get("cells") or len(initial["cells"]) < 100:
                raise AssertionError("Initial snapshot did not contain the dungeon view")
            if not isinstance(initial.get("player"), list) or len(initial["player"]) != 2:
                raise AssertionError(f"Invalid player position: {initial.get('player')!r}")

            client.sendall(b"WAIT\n")
            updated, buffer = receive_state(client, buffer)
            if updated.get("type") != "state":
                raise AssertionError(f"Expected updated state, got: {updated!r}")
            if updated.get("turn", 0) <= initial.get("turn", -1):
                raise AssertionError(
                    f"WAIT did not advance the authoritative turn: "
                    f"{initial.get('turn')} -> {updated.get('turn')}"
                )

            print(
                "Live Angband round trip passed: "
                f"initial turn {initial['turn']}, updated turn {updated['turn']}, "
                f"{len(initial['cells'])} visible/remembered cells, "
                f"player {initial['player']} -> {updated['player']}."
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
                waited, _ = os.waitpid(pid, os.WNOHANG)
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
