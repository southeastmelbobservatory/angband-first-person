#!/usr/bin/env python3
"""Download and verify all CC0 3D meshes, 2K PBR textures, and audio cues
listed in assets/manifest.json into assets/raw/.

Uses Python 3 standard library only (no external pip dependencies).
"""

import argparse
import importlib.util
import json
import os
import sys
import urllib.error
import urllib.request
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

USER_AGENT = "AngbandFirstPerson/1.0 (https://github.com/southeastmelbobservatory/angband-first-person)"


def fetch_json(url: str) -> dict:
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=30) as resp:
        return json.loads(resp.read().decode("utf-8"))


def verify_magic(path: Path) -> bool:
    if not path.is_file() or path.stat().st_size == 0:
        return False
    ext = path.suffix.lower()
    with path.open("rb") as f:
        head = f.read(32)
    if ext == ".glb":
        return head[:4] == b"glTF"
    if ext == ".gltf":
        return head.lstrip().startswith(b"{")
    if ext == ".fbx":
        return head.startswith(b"Kaydara FBX Binary") or b"FBX" in head
    if ext in (".jpg", ".jpeg"):
        return head[:2] == b"\xff\xd8"
    if ext == ".png":
        return head[:8] == b"\x89PNG\r\n\x1a\n"
    if ext == ".wav":
        return head[:4] == b"RIFF" and head[8:12] == b"WAVE"
    return True


def download_file(url: str, dest: Path, force: bool = False) -> tuple[str, int, str]:
    dest.parent.mkdir(parents=True, exist_ok=True)
    if not force and verify_magic(dest):
        return ("cached", dest.stat().st_size, str(dest))

    tmp = dest.with_suffix(dest.suffix + ".part")
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    try:
        with urllib.request.urlopen(req, timeout=60) as resp, tmp.open("wb") as out:
            while True:
                chunk = resp.read(65536)
                if not chunk:
                    break
                out.write(chunk)
        tmp.replace(dest)
    except Exception:
        if tmp.exists():
            tmp.unlink()
        raise

    if not verify_magic(dest):
        size = dest.stat().st_size if dest.exists() else 0
        dest.unlink(missing_ok=True)
        raise RuntimeError(f"Downloaded file failed format verification ({size} bytes): {dest}")

    return ("downloaded", dest.stat().st_size, str(dest))


def resolve_polyhaven_texture_tasks(entry: dict, raw_root: Path) -> list[tuple[str, Path]]:
    asset_id = entry["id"]
    res = entry.get("resolution", "2k")
    fmt = entry.get("format", "jpg")
    files_info = fetch_json(f"https://api.polyhaven.com/files/{asset_id}")
    dest_dir = raw_root / entry["dest_dir"]
    tasks: list[tuple[str, Path]] = []
    for map_key in entry.get("maps", ["Diffuse", "nor_dx", "arm"]):
        map_info = files_info.get(map_key, {}).get(res, {})
        fmt_info = map_info.get(fmt) or (next(iter(map_info.values())) if map_info else None)
        if not fmt_info or "url" not in fmt_info:
            raise RuntimeError(f"Poly Haven texture {asset_id} missing map {map_key} ({res}/{fmt})")
        url = fmt_info["url"]
        filename = os.path.basename(url.split("?")[0])
        tasks.append((url, dest_dir / filename))
    return tasks


def resolve_polyhaven_model_tasks(entry: dict, raw_root: Path) -> list[tuple[str, Path]]:
    asset_id = entry["id"]
    res = entry.get("resolution", "2k")
    fmt = entry.get("format", "gltf")
    files_info = fetch_json(f"https://api.polyhaven.com/files/{asset_id}")
    fmt_info = files_info.get(fmt, {}).get(res, {}).get(fmt)
    if not fmt_info or "url" not in fmt_info:
        raise RuntimeError(f"Poly Haven model {asset_id} missing {fmt}/{res}")
    dest_dir = raw_root / entry["dest_dir"]
    main_url = fmt_info["url"]
    tasks: list[tuple[str, Path]] = [
        (main_url, dest_dir / os.path.basename(main_url.split("?")[0]))
    ]
    for rel_path, inc_info in fmt_info.get("include", {}).items():
        if "url" in inc_info:
            tasks.append((inc_info["url"], dest_dir / rel_path))
    return tasks


def main() -> int:
    parser = argparse.ArgumentParser(description="Fetch CC0 graphics and audio assets for Angband: First Person.")
    parser.add_argument("--manifest", default=None, help="Path to manifest.json (default: assets/manifest.json)")
    parser.add_argument("--category", choices=["all", "materials", "models", "terrain", "creatures", "items", "audio"], default="all")
    parser.add_argument("--force", action="store_true", help="Re-download existing files")
    parser.add_argument("--dry-run", action="store_true", help="Resolve URLs without downloading")
    parser.add_argument("--jobs", type=int, default=6, help="Concurrent download threads")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    manifest_path = Path(args.manifest) if args.manifest else (repo_root / "assets" / "manifest.json")
    raw_root = repo_root / "assets" / "raw"

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    tasks: list[tuple[str, Path]] = []

    def include_cat(name: str) -> bool:
        return args.category in ("all", name)

    if include_cat("materials"):
        print("Resolving Poly Haven 2K PBR texture maps...")
        for entry in manifest.get("pbr_materials", []):
            tasks.extend(resolve_polyhaven_texture_tasks(entry, raw_root))

    if include_cat("models"):
        print("Resolving Poly Haven 2K 3D hero models...")
        for entry in manifest.get("polyhaven_models", []):
            tasks.extend(resolve_polyhaven_model_tasks(entry, raw_root))

    if include_cat("terrain"):
        feat_gen = repo_root / "tools" / "generate-dungeon-features.py"
        if feat_gen.is_file() and not args.dry_run:
            spec = importlib.util.spec_from_file_location("abfp_gen_features", str(feat_gen))
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            n_feat = mod.generate_all(raw_root)
            print(f"Generated {n_feat} bespoke dungeon architecture GLBs.")
        for entry in manifest.get("terrain_meshes", []):
            if entry.get("source") == "procedural" or not entry.get("url"):
                continue
            tasks.append((entry["url"], raw_root / entry["dest"]))

    if include_cat("creatures"):
        gen_path = repo_root / "tools" / "generate-angband-creatures.py"
        if gen_path.is_file() and not args.dry_run:
            spec = importlib.util.spec_from_file_location("abfp_gen_creatures", str(gen_path))
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            n_gen = mod.generate_all(raw_root)
            print(f"Generated {n_gen} bespoke procedural Angband dungeon creature GLBs.")
        for entry in manifest.get("creatures", []):
            if entry.get("source") == "procedural" or not entry.get("url"):
                continue
            tasks.append((entry["url"], raw_root / entry["dest"]))

    if include_cat("items"):
        item_gen = repo_root / "tools" / "generate-angband-items.py"
        if item_gen.is_file() and not args.dry_run:
            spec = importlib.util.spec_from_file_location("abfp_gen_items", str(item_gen))
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            n_items = mod.generate_all(raw_root)
            print(f"Generated {n_items} bespoke Angband item & viewmodel GLBs.")
        for entry in manifest.get("item_meshes", []):
            if entry.get("source") == "procedural" or not entry.get("url"):
                continue
            tasks.append((entry["url"], raw_root / entry["dest"]))
        for entry in manifest.get("support_textures", []):
            tasks.append((entry["url"], raw_root / entry["dest"]))

    if include_cat("audio"):
        for entry in manifest.get("audio", []):
            tasks.append((entry["url"], raw_root / entry["dest"]))

    print(f"Resolved {len(tasks)} asset files targeting {raw_root}")
    if args.dry_run:
        for url, dest in tasks:
            print(f"  {dest.relative_to(raw_root)} <- {url}")
        return 0

    downloaded = 0
    cached = 0
    total_bytes = 0
    failures: list[tuple[str, str]] = []

    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        future_map = {
            pool.submit(download_file, url, dest, args.force): (url, dest)
            for url, dest in tasks
        }
        for future in as_completed(future_map):
            url, dest = future_map[future]
            rel = dest.relative_to(raw_root)
            try:
                status, size, _ = future.result()
                total_bytes += size
                if status == "downloaded":
                    downloaded += 1
                    print(f"  [OK]     {rel} ({size / 1024:.1f} KiB)")
                else:
                    cached += 1
            except Exception as exc:
                failures.append((str(rel), f"{exc} ({url})"))
                print(f"  [FAIL]   {rel}: {exc}", file=sys.stderr)

    print(
        f"\nSummary: {downloaded} downloaded, {cached} cached, {len(failures)} failed "
        f"({total_bytes / (1024 * 1024):.1f} MiB total in assets/raw/)"
    )
    if failures:
        print("\nFailed assets:", file=sys.stderr)
        for rel, msg in failures:
            print(f"  - {rel}: {msg}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
