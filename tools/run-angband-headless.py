#!/usr/bin/env python3
"""Run the patched Angband host headlessly in the background for 3D clients.

Automates Angband's ncurses startup/character-birth flow inside a PTY so the
ABFP TCP bridge reaches CTX_GAME and starts serving Unreal immediately with no
manual terminal input. Immune to Windows CRLF line endings when invoked from WSL.
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
import tempfile
import termios
import time
from pathlib import Path

DEFAULT_PORT = 47652
UPSTREAM_URL = "https://github.com/angband/angband.git"
UPSTREAM_COMMIT = "f3082213b73f3e463e3d0d60bff4b00462beae6e"


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
    for tool in ("git", "gcc", "cmake", "pkg-config"):
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


def bootstrap_upstream_angband(repo_root: Path) -> Path:
    dest = repo_root / "engine" / "upstream" / "angband"
    if (dest / ".git").is_dir():
        head = subprocess.run(
            ["git", "-C", str(dest), "rev-parse", "HEAD"],
            capture_output=True,
            text=True,
            check=False,
        ).stdout.strip()
        if head != UPSTREAM_COMMIT:
            print("[abfp-host] Fetching upstream Angband tags...", flush=True)
            subprocess.run(["git", "-C", str(dest), "fetch", "--tags"], check=True)
    else:
        dest.parent.mkdir(parents=True, exist_ok=True)
        print("[abfp-host] Cloning pinned Angband 4.2.6 source...", flush=True)
        subprocess.run(
            ["git", "clone", "--filter=blob:none", UPSTREAM_URL, str(dest)],
            check=True,
        )

    head = subprocess.run(
        ["git", "-C", str(dest), "rev-parse", "HEAD"],
        capture_output=True,
        text=True,
        check=True,
    ).stdout.strip()
    if head != UPSTREAM_COMMIT:
        subprocess.run(["git", "-C", str(dest), "checkout", "--detach", UPSTREAM_COMMIT], check=True)

    # Normalize patch line endings to LF in a temp file so Windows core.autocrlf never breaks git apply.
    patch_src = repo_root / "engine" / "upstream" / "patches" / "angband-4.2.6-abfp.patch"
    patch_bytes = patch_src.read_bytes().replace(b"\r\n", b"\n")
    with tempfile.NamedTemporaryFile(prefix="abfp-patch-", suffix=".patch", delete=False) as tmp:
        tmp.write(patch_bytes)
        tmp_patch = Path(tmp.name)

    try:
        can_apply = subprocess.run(
            ["git", "-C", str(dest), "apply", "--check", str(tmp_patch)],
            capture_output=True,
            check=False,
        ).returncode == 0
        already_applied = subprocess.run(
            ["git", "-C", str(dest), "apply", "--reverse", "--check", str(tmp_patch)],
            capture_output=True,
            check=False,
        ).returncode == 0

        if can_apply:
            print("[abfp-host] Applying ABFP frontend integration patch...", flush=True)
            subprocess.run(["git", "-C", str(dest), "apply", str(tmp_patch)], check=True)
        elif already_applied:
            print("[abfp-host] ABFP patch already applied.", flush=True)
        else:
            print("[abfp-host] Updating ABFP patch in engine/upstream/angband...", flush=True)
            subprocess.run(["git", "-C", str(dest), "checkout", "--", "."], check=True)
            subprocess.run(["git", "-C", str(dest), "apply", str(tmp_patch)], check=True)
    finally:
        tmp_patch.unlink(missing_ok=True)

    return dest


def sources_newer_than(repo_root: Path, binary: Path) -> bool:
    bin_mtime = binary.stat().st_mtime
    patch_src = repo_root / "engine" / "upstream" / "patches" / "angband-4.2.6-abfp.patch"
    if patch_src.is_file() and patch_src.stat().st_mtime > bin_mtime:
        return True
    for folder in (repo_root / "engine" / "bridge", repo_root / "engine" / "host", repo_root / "protocol"):
        if not folder.exists():
            continue
        for src in folder.rglob("*"):
            if src.suffix in (".c", ".h") and src.stat().st_mtime > bin_mtime:
                return True
    return False


def ensure_angband_built(repo_root: Path, build_dir: Path, rebuild: bool) -> tuple[Path, Path]:
    upstream_dir = repo_root / "engine" / "upstream" / "angband"
    binary = build_dir / "game" / "angband"
    if (
        binary.is_file()
        and os.access(binary, os.X_OK)
        and (upstream_dir / "lib" / "screens" / "news.txt").is_file()
        and not rebuild
        and not sources_newer_than(repo_root, binary)
    ):
        return binary, upstream_dir

    missing = check_linux_prerequisites()
    if missing:
        print(
            f"[abfp-host] Missing Linux build prerequisites: {' '.join(missing)}\n"
            "Install them once in Linux/WSL with:\n"
            "  wsl -u root -- bash -c \"apt update && apt install -y build-essential cmake pkg-config libncurses-dev\"",
            file=sys.stderr,
            flush=True,
        )
        raise SystemExit(1)

    upstream_dir = bootstrap_upstream_angband(repo_root)
    build_dir.mkdir(parents=True, exist_ok=True)

    jobs = os.environ.get("ABFP_BUILD_JOBS") or str(max(2, os.cpu_count() or 2))
    print(f"[abfp-host] Configuring & building Angband in {build_dir} ({jobs} jobs)...", flush=True)
    subprocess.run(
        [
            "cmake",
            "-S", str(upstream_dir),
            "-B", str(build_dir),
            "-DSUPPORT_GCU_FRONTEND=ON",
            "-DSUPPORT_X11_FRONTEND=OFF",
            "-DSUPPORT_SPOIL_FRONTEND=OFF",
            "-DSUPPORT_BORG=OFF",
        ],
        check=True,
    )
    subprocess.run(
        ["cmake", "--build", str(build_dir), "--parallel", jobs],
        check=True,
    )
    if not binary.is_file():
        raise RuntimeError(f"Expected Angband binary was not produced at {binary}")
    return binary, upstream_dir


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
    except (OSError, ValueError):
        return None
    return None


def detect_resume_save_name(save_dir: Path, user_dir: Path) -> str | None:
    """Find the most recent living Angband savefile to auto-resume on startup."""
    def clean_basename(name: str) -> str:
        # Strip optional POSIX SETGID "<uid>." prefix if present
        m = re.match(r"^\d+\.(.+)$", name)
        return m.group(1) if m else name

    def is_living_save_file(path: Path) -> bool:
        if not path.is_file() or path.stat().st_size < 32:
            return False
        if path.name.startswith(".") or path.name.endswith((".new", ".old", ".panic")):
            return False
        try:
            head = path.read_bytes()[:4096]
            if b", dead (" in head:
                return False
        except OSError:
            return False
        return True

    def find_matching_file(clean_name: str) -> Path | None:
        if not clean_name:
            return None
        for candidate in save_dir.iterdir():
            if is_living_save_file(candidate) and clean_basename(candidate.name) == clean_name:
                return candidate
        return None

    marker = user_dir / "abfp_last_save.txt"
    if marker.is_file():
        try:
            last_name = marker.read_text(encoding="utf-8", errors="ignore").strip().splitlines()[0].strip()
            if find_matching_file(last_name) is not None:
                return last_name
        except (OSError, IndexError):
            pass

    living: list[tuple[float, str]] = []
    if save_dir.is_dir():
        for entry in save_dir.iterdir():
            if is_living_save_file(entry):
                try:
                    living.append((entry.stat().st_mtime, clean_basename(entry.name)))
                except OSError:
                    pass
    if living:
        living.sort(reverse=True)
        return living[0][1]
    return None


def request_graceful_save(host: str, port: int, timeout: float = 0.75) -> bool:
    """Ask the running ABFP host to flush a savefile cleanly before shutdown."""
    try:
        with socket.create_connection((host, port), timeout=timeout) as sock:
            sock.settimeout(timeout)
            sock.sendall(b"SAVE\n")
            buf = b""
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                chunk = sock.recv(65536)
                if not chunk:
                    break
                buf += chunk
                if b"\n" in buf:
                    return True
    except OSError:
        return False
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description="Run patched Angband host headlessly in the background.")
    parser.add_argument("--bind", default=os.environ.get("ABFP_BIND_ADDRESS", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--player", default="ABFPPlayer", help="Angband character/save name")
    parser.add_argument("--save-root", default="", help="Persistent save/user root directory")
    parser.add_argument("--new-game", action="store_true", help="Start a brand-new character via in-client 3D character creation")
    parser.add_argument("--auto-birth", action="store_true", help="Auto-roll a random character instead of opening in-client character creation")
    parser.add_argument("--rebuild", action="store_true", help="Force rebuilding the Angband host binary")
    parser.add_argument("--pid-file", default="", help="Optional PID file path")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent

    # When running in WSL against a /mnt/* Windows path, keep the CMake build
    # directory on native Linux ext4 for 10x faster compiles and proper POSIX perms.
    if os.environ.get("ABFP_BUILD_DIR"):
        build_dir = Path(os.environ["ABFP_BUILD_DIR"])
    elif str(repo_root).startswith("/mnt/"):
        build_dir = Path.home() / ".cache" / "angband-first-person" / "build-angband"
    else:
        build_dir = repo_root / "build-angband"

    binary, upstream_dir = ensure_angband_built(repo_root, build_dir, args.rebuild)

    if not (upstream_dir / "lib" / "screens" / "news.txt").is_file():
        print(f"[abfp-host] Missing Angband data directory: {upstream_dir / 'lib'}", file=sys.stderr, flush=True)
        return 1

    probe_host = "127.0.0.1" if args.bind in ("0.0.0.0", "") else args.bind

    existing = probe_initial_snapshot(probe_host, args.port, timeout=0.6)
    if existing is not None:
        print(
            f"[abfp-host] READY on {probe_host}:{args.port} (already running) | "
            f"depth={existing.get('depth')} turn={existing.get('turn')}",
            flush=True,
        )
        return 0

    save_root = Path(args.save_root) if args.save_root else (Path.home() / ".local" / "share" / "angband-first-person")
    user_dir = save_root / "user"
    save_dir = save_root / "save"
    user_dir.mkdir(parents=True, exist_ok=True)
    save_dir.mkdir(parents=True, exist_ok=True)

    player_name = args.player
    if not args.new_game and player_name == "ABFPPlayer":
        resumed = detect_resume_save_name(save_dir, user_dir)
        if resumed:
            player_name = resumed
            print(f"[abfp-host] Auto-resuming saved hero: {player_name}", flush=True)

    pid_file = Path(args.pid_file) if args.pid_file else (save_root / "angband-host.pid")

    argv = [
        str(binary),
        f"-u{player_name}",
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
        if not args.auto_birth:
            env["ABFP_CLIENT_BIRTH"] = "1"
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

            if "panic save exists" in screen:
                os.write(master_fd, b"y")
                output.clear()
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

        mode_label = "character-creation" if initial_snapshot.get("birth") else "in-game"
        print(
            f"[abfp-host] READY on {args.bind}:{args.port} ({mode_label}) | "
            f"player={player_name} depth={initial_snapshot.get('depth')} "
            f"turn={initial_snapshot.get('turn')} pos={initial_snapshot.get('player')}",
            flush=True,
        )

        while not stopping:
            waited, status = os.waitpid(pid, os.WNOHANG)
            if waited:
                print(f"[abfp-host] Angband process exited (status={status}).", flush=True)
                break
            before_len = len(output)
            drain_pty(master_fd, output, 0.25)
            if len(output) > before_len:
                tail = strip_ansi(bytes(output[-2048:]))
                if any(tok in tail for tok in ("esc) exit", "purchase which item", "sell which item", "(y/n)", "[y/n]")):
                    os.write(master_fd, b"\x1b")
                    output.clear()
                elif "-more-" in tail or "press any key" in tail:
                    os.write(master_fd, b" ")
                    output.clear()

    finally:
        # Flush a clean Angband save before terminating the game process
        request_graceful_save(probe_host, args.port, timeout=0.75)
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
