#!/usr/bin/env python3
"""Verify the patched Angband process starts its ABFP loopback listener."""

import fcntl
import os
import pty
import signal
import socket
import struct
import tempfile
import termios
import time
import sys

def main():
    binary = os.path.abspath(sys.argv[1])
    with tempfile.TemporaryDirectory(prefix="abfp-host-smoke-") as home:
        user_dir = os.path.join(home, "user")
        save_dir = os.path.join(home, "save")
        os.makedirs(user_dir)
        os.makedirs(save_dir)

        pid, master = pty.fork()
        if pid == 0:
            env = os.environ.copy()
            env.update({
                "HOME": home,
                "TERM": "xterm-256color",
                "LANG": "C.UTF-8",
                "LC_ALL": "C.UTF-8",
            })
            os.execve(binary, [
                binary,
                "-n",
                "-uABFPRuntime",
                f"-duser={user_dir}",
                f"-dsave={save_dir}",
            ], env)

        fcntl.ioctl(master, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 100, 0, 0))
        os.set_blocking(master, False)
        output = bytearray()
        deadline = time.monotonic() + 20
        connected = False

        try:
            while time.monotonic() < deadline:
                try:
                    chunk = os.read(master, 4096)
                    if chunk:
                        output.extend(chunk)
                except (BlockingIOError, OSError):
                    pass

                waited, status = os.waitpid(pid, os.WNOHANG)
                if waited:
                    raise RuntimeError(
                        "Angband exited before opening its ABFP listener:\n"
                        + output.decode(errors="replace")[-4000:]
                    )

                try:
                    with socket.create_connection(("127.0.0.1", 47652), timeout=0.25):
                        connected = True
                    break
                except OSError:
                    time.sleep(0.1)

            if not connected:
                raise RuntimeError(
                    "Timed out waiting for Angband's ABFP listener:\n"
                    + output.decode(errors="replace")[-4000:]
                )
        finally:
            try:
                os.kill(pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                os.waitpid(pid, 0)
            except ChildProcessError:
                pass
            os.close(master)

    print("ABFP patched Angband host started and accepted a loopback TCP connection.")

if __name__ == "__main__":
    main()
