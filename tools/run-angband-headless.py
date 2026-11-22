#!/usr/bin/env python3
"""Run the patched Angband host headlessly in the background for 3D clients.

Automates Angband's ncurses startup/character-birth flow inside a PTY so the
ABFP TCP bridge reaches CTX_GAME and starts serving Unreal immediately with no
manual terminal input.
"""

import argparse
import fcntl
import json
import os
import pty
import re
import select
import shutil
import signal
import socket
import struct
import subprocess
import sys
import termios
import time
from pathlib import Path

DEFAULT_PORT = 47652


def strip_ansi(raw: bytes) -> str:
    text = raw.decode("utf-8", errors="replace")
    return re.sub(
        r"\x1b(?:\[[0-?]*[ -/]*[@-~]|\][^\x07]*(?:\x07|\x1b\\)|[()][A-Z0-9])",
        "",
        text,
    ).lower()


def drain_pty(master_fd: int, output: bytearray, wait_seconds: float = 0.05) -> None:
    readable, _, _ = select.select([master_fd], [], [], wait_seconds)
    if not readable:
        return
    try:
        while True:
            chunk = os.read(master_fd, 8192)
            if not chunk:
                break
            output.extend(chunk)
            if len(output) > 65536:
                del output[:-32768]
    except BlockingIOError:
        pass
    except OSError:
        pass


def check_linux_prerequisites() -> list[str]:
    missing = []
    for tool in ("gcc", "cmake", "pkg-config"):
        if shutil.which(tool) is None:
            missing.append(tool)
    if shutil.which("pkg-config") is not None:
        has_ncurses = (
            subprocess.run(["pkg-config", "--exists", "ncurses"], check=False).returncode == 0
            or subprocess.run(["pkg-config", "--exists", "ncursesw"], check=False).returncode == 0
        )
        if not has_ncurses:
            missing.append("libncurses-dev")
    return missing


def ensure_angband_built(repo_root: Path, build_dir: Path, rebuild: bool) -> Path:
    binary = build_dir / "game" / "angband"
    if binary.is_file() and os.access(binary, os.X_OK) and not rebuild:
        return binary

    missing = check_linux_prerequisites()
    if missing:
        print(
            f"[abfp-host] Missing Linux build prerequisites: {' '.join(missing)}\n"
            "Install them once in Linux/WSL with:\n"
            "  sudo apt update && sudo apt install -y build-essential cmake pkg-config libncurses-dev",
            file=sys.stderr,
            flush=True,
        )
        raise SystemExit(1)

    print("[abfp-host] Building patched Angband 4.2.6 host...", flush=True)
    env = os.environ.copy()
    env["ABFP_BUILD_DIR"] = str(build_dir)
    subprocess.run(
        ["bash", str(repo_root / "tools" / "build-angband-host.sh")],
        cwd=str(repo_root),
        env=env,
        check=True,
    )
    if not binary.is_file():
        raise RuntimeError(f"Expected Angband binary was not produced at {binary}")
    return binary


def probe_initial_snapshot(host: str, port: int, timeout: float = 1.5) -> dict | None:
    try:
        with socket.create_connection((host, port), timeout=timeout) as sock:
            sock.settimeout(timeout)
            buf = b""
            deadline = time.monotonic() + timeout
            while b"\n" not in buf and time.monotonic() < deadline:
                chunk = sock.recv(65536)
                if not chunk:
                    return None
                buf += chunk
            if b"\n" not in buf:
                return None
            line, _, _ = buf.partition(b"\n")
            data = json.loads(line.decode("utf-8", errors="replace"))
            if data.get("type") == "state":
                return data
    except OSError:
        return None
    except ValueError:
        return None
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description="Run patched Angband host headlessly in the background.")
    parser.add_argument("--bind", default=os.environ.get("ABFP_BIND_ADDRESS", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--player", default="ABFPPlayer", help="Angband character/save name")
    parser.add_argument("--save-root", default="", help="Persistent save/user root directory")
    parser.add_argument("--new-game", action="store_true", help="Force a brand-new random character")
    parser.add_argument("--rebuild", action="store_true", help="Force rebuilding the Angband host binary")
    parser.add_argument("--pid-file", default="", help="Optional PID file path")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    build_dir = Path(os.environ.get("ABFP_BUILD_DIR", str(repo_root / "build-angband")))
    binary = ensure_angband_built(repo_root, build_dir, args.rebuild)
    upstream_dir = repo_root / "engine" / "upstream" / "angband"

    if not (upstream_dir / "lib" / "screens" / "news.txt").is_file():
        print(f"[abfp-host] Missing Angband data directory: {upstream_dir / 'lib'}", file=sys.stderr, flush=True)
        return 1

    probe_host = "127.0.0.1" if args.bind in ("0.0.0.0", "") else args.bind

    # If a host is already serving on this port, report and exit cleanly.
    existing = probe_initial_snapshot(probe_host, args.port, timeout=0.6)
    if existing is not None:
        print(
            f"[abfp-host] Angband host already listening on {probe_host}:{args.port} "
            f"(depth={existing.get('depth')} turn={existing.get('turn')})",
            flush=True,
        )
        return 0

    save_root = Path(args.save_root) if args.save_root else (Path.home() / ".local" / "share" / "angband-first-person")
    user_dir = save_root / "user"
    save_dir = save_root / "save"
    user_dir.mkdir(parents=True, exist_ok=True)
    save_dir.mkdir(parents=True, exist_ok=True)

    pid_file = Path(args.pid_file) if args.pid_file else (save_root / "angband-host.pid")

    argv = [
        str(binary),
        f"-u{args.player}",
        f"-duser={user_dir}",
        f"-dsave={save_dir}",
    ]
    if args.new_game:
        argv.insert(1, "-n")

    pid, master_fd = pty.fork()
    if pid == 0:
        os.chdir(str(upstream_dir))
        env = os.environ.copy()
        env.update({
            "TERM": "xterm-256color",
            "LANG": "C.UTF-8",
            "LC_ALL": "C.UTF-8",
            "ABFP_BIND_ADDRESS": args.bind,
        })
        os.execve(str(binary), argv, env)

    fcntl.ioctl(master_fd, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 100, 0, 0))
    os.set_blocking(master_fd, False)

    stopping = False

    def handle_signal(signum, _frame):
        nonlocal stopping
        stopping = True

    signal.signal(signal.SIGTERM, handle_signal)
    signal.signal(signal.SIGINT, handle_signal)

    pid_file.write_text(f"{os.getpid()}\n{pid}\n", encoding="utf-8")
    output = bytearray()

    try:
        # 1. Wait for Angband's ABFP TCP socket to open (before startup splash).
        listener_ready = False
        deadline = time.monotonic() + 20.0
        while time.monotonic() < deadline and not stopping:
            waited, status = os.waitpid(pid, os.WNOHANG)
            if waited:
                print(
                    f"[abfp-host] Angband exited early (status={status}):\n{strip_ansi(bytes(output))[-2000:]}",
                    file=sys.stderr,
                    flush=True,
                )
                return 1
            drain_pty(master_fd, output, 0.1)
            try:
                with socket.create_connection((probe_host, args.port), timeout=0.25):
                    listener_ready = True
                    break
            except OSError:
                pass

        if not listener_ready:
            print("[abfp-host] Timed out waiting for ABFP TCP listener.", file=sys.stderr, flush=True)
            return 1

        # 2. Drive splash & birth screens automatically until CTX_GAME publishes a snapshot.
        time.sleep(0.25)
        os.write(master_fd, b" ")

        sent_quickroll = False
        sent_confirm = False
        initial_snapshot = None
        birth_deadline = time.monotonic() + 25.0

        while time.monotonic() < birth_deadline and not stopping:
            waited, status = os.waitpid(pid, os.WNOHANG)
            if waited:
                print(
                    f"[abfp-host] Angband exited during startup (status={status}):\n{strip_ansi(bytes(output))[-2000:]}",
                    file=sys.stderr,
                    flush=True,
                )
                return 1

            drain_pty(master_fd, output, 0.15)
            screen = strip_ansi(bytes(output))

            if not sent_quickroll and "finish the character with random selections" in screen:
                os.write(master_fd, b"@")
                sent_quickroll = True
                continue

            if sent_quickroll and not sent_confirm and "any other key to continue" in screen:
                time.sleep(0.25)
                os.write(master_fd, b"x")
                sent_confirm = True
                continue

            if "press any key" in screen or "-more-" in screen:
                os.write(master_fd, b" ")

            initial_snapshot = probe_initial_snapshot(probe_host, args.port, timeout=0.35)
            if initial_snapshot is not None:
                break

        if initial_snapshot is None:
            print(
                "[abfp-host] Timed out waiting for Angband to enter CTX_GAME.\n"
                f"Terminal tail:\n{strip_ansi(bytes(output))[-2500:]}",
                file=sys.stderr,
                flush=True,
            )
            return 1

        print(
            f"[abfp-host] READY on {args.bind}:{args.port} | "
            f"player={args.player} depth={initial_snapshot.get('depth')} "
            f"turn={initial_snapshot.get('turn')} pos={initial_snapshot.get('player')}",
            flush=True,
        )

        # 3. Stay running quietly, continuously draining the PTY so Angband never blocks.
        while not stopping:
            waited, status = os.waitpid(pid, os.WNOHANG)
            if waited:
                print(f"[abfp-host] Angband process exited (status={status}).", flush=True)
                break
            drain_pty(master_fd, output, 0.25)

    finally:
        try:
            os.kill(pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        deadline = time.monotonic() + 3.0
        while time.monotonic() < deadline:
            try:
                waited, _ = os.waitpid(pid, os.WNOHANG)
                if waited:
                    break
            except ChildProcessError:
                break
            time.sleep(0.05)
        else:
            try:
                os.kill(pid, signal.SIGKILL)
                os.waitpid(pid, 0)
            except (ProcessLookupError, ChildProcessError):
                pass
        try:
            os.close(master_fd)
        except OSError:
            pass
        try:
            pid_file.unlink(missing_ok=True)
        except OSError:
            pass

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
