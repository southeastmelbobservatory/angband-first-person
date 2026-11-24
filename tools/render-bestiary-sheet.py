#!/usr/bin/env python3
"""Render real textured 3D preview thumbnails for Angband: First Person's bestiary
and generate a visual HTML verification sheet for Town (Depth 0) through Depth 5.

Uses pure Python + NumPy + ffmpeg (no Blender or Pillow required).
"""

import argparse
import base64
import importlib.util
import json
import math
import re
import struct
import subprocess
import urllib.request
import zlib
from pathlib import Path

import numpy as np


def load_merger(repo_root: Path):
    spec = importlib.util.spec_from_file_location("abfp_merge_kaykit", str(repo_root / "tools" / "merge-kaykit-glb.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod.merge_kaykit_glb


def write_png(path: Path, rgb: np.ndarray):
    h, w, _ = rgb.shape
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw.extend(rgb[y].tobytes())

    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 6)) + chunk(b"IEND", b"")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)


def decode_image_bytes(img_bytes: bytes):
    """Decode PNG/JPEG bytes into an (H, W, 3) float32 [0..1] array using ffmpeg."""
    try:
        # First probe dimensions via ffprobe or ffmpeg pipe
        proc = subprocess.run(
            ["ffmpeg", "-hide_banner", "-loglevel", "error", "-i", "pipe:0",
             "-vf", "scale=256:256:flags=bilinear", "-f", "rawvideo", "-pix_fmt", "rgb24", "pipe:1"],
            input=img_bytes,
            capture_output=True,
            timeout=10,
        )
        if proc.returncode == 0 and len(proc.stdout) == 256 * 256 * 3:
            return np.frombuffer(proc.stdout, dtype=np.uint8).reshape((256, 256, 3)).astype(np.float32) / 255.0
    except Exception:
        pass
    return None


def mat4_mul(a, b):
    return a @ b


def node_matrix(node):
    if "matrix" in node:
        return np.array(node["matrix"], dtype=np.float64).reshape((4, 4)).T
    t = node.get("translation", [0.0, 0.0, 0.0])
    r = node.get("rotation", [0.0, 0.0, 0.0, 1.0])
    s = node.get("scale", [1.0, 1.0, 1.0])
    qx, qy, qz, qw = r
    sx, sy, sz = s
    xx, yy, zz = qx * qx, qy * qy, qz * qz
    xy, xz, yz = qx * qy, qx * qz, qy * qz
    wx, wy, wz = qw * qx, qw * qy, qw * qz
    m = np.array([
        [(1 - 2 * (yy + zz)) * sx, (2 * (xy - wz)) * sy, (2 * (xz + wy)) * sz, t[0]],
        [(2 * (xy + wz)) * sx, (1 - 2 * (xx + zz)) * sy, (2 * (yz - wx)) * sz, t[1]],
        [(2 * (xz - wy)) * sx, (2 * (yz + wx)) * sy, (1 - 2 * (xx + yy)) * sz, t[2]],
        [0.0, 0.0, 0.0, 1.0],
    ], dtype=np.float64)
    return m


def read_accessor_np(gltf, buffers, acc_idx):
    acc = gltf["accessors"][acc_idx]
    bv = gltf["bufferViews"][acc["bufferView"]]
    buf = buffers[bv["buffer"]]
    count = acc["count"]
    comp_map = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
    n_map = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}
    dtype = np.dtype(comp_map[acc["componentType"]]).newbyteorder("<")
    n_comp = n_map[acc["type"]]
    elem_bytes = dtype.itemsize * n_comp
    stride = bv.get("byteStride", elem_bytes)
    offset = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    if stride == elem_bytes:
        arr = np.frombuffer(buf, dtype=dtype, count=count * n_comp, offset=offset).reshape((count, n_comp))
    else:
        raw = np.frombuffer(buf, dtype=np.uint8, count=count * stride, offset=offset).reshape((count, stride))
        arr = np.frombuffer(np.ascontiguousarray(raw[:, :elem_bytes]), dtype=dtype).reshape((count, n_comp))
    return arr.astype(np.float32) if acc["componentType"] == 5126 else arr


def load_gltf_mesh(path: Path):
    if path.suffix.lower() == ".glb":
        data = path.read_bytes()
        jlen = struct.unpack_from("<I", data, 12)[0]
        gltf = json.loads(data[20 : 20 + jlen].decode("utf-8"))
        bin_off = 20 + jlen + 8
        bin_len = struct.unpack_from("<I", data, 20 + jlen)[0]
        buffers = [data[bin_off : bin_off + bin_len]]
    else:
        gltf = json.loads(path.read_text(encoding="utf-8"))
        buffers = []
        for b in gltf.get("buffers", []):
            uri = b.get("uri", "")
            if uri.startswith("data:"):
                buffers.append(base64.b64decode(uri.split(",", 1)[1]))
            else:
                buffers.append((path.parent / uri).read_bytes())

    nodes = gltf.get("nodes", [])
    scenes = gltf.get("scenes", [])
    roots = scenes[gltf.get("scene", 0)].get("nodes", []) if scenes else list(range(len(nodes)))
    world = {}

    def walk(idx, parent):
        m = parent @ node_matrix(nodes[idx])
        world[idx] = m
        for ch in nodes[idx].get("children", []):
            walk(ch, m)

    for r in roots:
        walk(r, np.eye(4))

    # Decode first texture image if available
    tex_img = None
    images = gltf.get("images", [])
    if images:
        im0 = images[0]
        raw_img = None
        if "bufferView" in im0:
            bv = gltf["bufferViews"][im0["bufferView"]]
            off = bv.get("byteOffset", 0)
            raw_img = buffers[bv["buffer"]][off : off + bv["byteLength"]]
        elif "uri" in im0:
            uri = im0["uri"]
            if uri.startswith("data:"):
                raw_img = base64.b64decode(uri.split(",", 1)[1])
            elif (path.parent / uri).is_file():
                raw_img = (path.parent / uri).read_bytes()
        if raw_img:
            tex_img = decode_image_bytes(raw_img)

    tris_v = []
    tris_n = []
    tris_uv = []
    tris_vc = []
    mat_colors = []
    for m in gltf.get("materials", []):
        c = m.get("pbrMetallicRoughness", {}).get("baseColorFactor", [0.75, 0.75, 0.75, 1.0])
        mat_colors.append(np.array(c[:3], dtype=np.float32))

    for idx, node in enumerate(nodes):
        if "mesh" not in node:
            continue
        m = world.get(idx, np.eye(4))
        rot = m[:3, :3]
        mesh = gltf["meshes"][node["mesh"]]
        for prim in mesh.get("primitives", []):
            attrs = prim.get("attributes", {})
            if "POSITION" not in attrs:
                continue
            pos = read_accessor_np(gltf, buffers, attrs["POSITION"]).astype(np.float64)
            pos_h = np.concatenate([pos, np.ones((len(pos), 1))], axis=1)
            wpos = (pos_h @ m.T)[:, :3].astype(np.float32)

            if "NORMAL" in attrs:
                nor = read_accessor_np(gltf, buffers, attrs["NORMAL"]).astype(np.float64)
                wnor = (nor @ rot.T).astype(np.float32)
                nlen = np.linalg.norm(wnor, axis=1, keepdims=True)
                wnor = np.where(nlen > 1e-6, wnor / np.maximum(nlen, 1e-6), np.array([0.0, 1.0, 0.0], dtype=np.float32))
            else:
                wnor = np.tile(np.array([0.0, 1.0, 0.0], dtype=np.float32), (len(pos), 1))

            if "TEXCOORD_0" in attrs:
                uv = read_accessor_np(gltf, buffers, attrs["TEXCOORD_0"]).astype(np.float32)
            else:
                uv = np.zeros((len(pos), 2), dtype=np.float32)

            if "COLOR_0" in attrs:
                vc = read_accessor_np(gltf, buffers, attrs["COLOR_0"]).astype(np.float32)
                if vc.max() > 1.5:
                    vc = vc / 255.0
                vc = vc[:, :3]
            else:
                mi = prim.get("material", 0)
                mc = mat_colors[mi] if mi < len(mat_colors) else np.array([0.75, 0.75, 0.75], dtype=np.float32)
                vc = np.tile(mc, (len(pos), 1))

            if "indices" in prim:
                inds = read_accessor_np(gltf, buffers, prim["indices"]).reshape(-1).astype(np.int32)
            else:
                inds = np.arange(len(pos), dtype=np.int32)
            if len(inds) < 3:
                continue
            tri_idx = inds[: (len(inds) // 3) * 3].reshape((-1, 3))
            tris_v.append(wpos[tri_idx])
            tris_n.append(wnor[tri_idx])
            tris_uv.append(uv[tri_idx])
            tris_vc.append(vc[tri_idx])

    if not tris_v:
        return None
    V = np.concatenate(tris_v, axis=0)
    N = np.concatenate(tris_n, axis=0)
    UV = np.concatenate(tris_uv, axis=0)
    VC = np.concatenate(tris_vc, axis=0)
    # Decimate huge meshes slightly for ultra-fast preview rendering while preserving silhouette & texture
    if len(V) > 25000:
        step = max(1, len(V) // 25000)
        V, N, UV, VC = V[::step], N[::step], UV[::step], VC[::step]
    return V, N, UV, VC, tex_img


def render_mesh_thumbnail(path: Path, out_png: Path, entry: dict, size: int = 160):
    loaded = load_gltf_mesh(path)
    img = np.zeros((size, size, 3), dtype=np.float32)
    # Subtle dark slate gradient backdrop
    grad = np.linspace(0.16, 0.08, size, dtype=np.float32)[:, None, None]
    img[:, :, 0] = grad[:, :, 0] * 0.9
    img[:, :, 1] = grad[:, :, 0] * 1.0
    img[:, :, 2] = grad[:, :, 0] * 1.25
    if loaded is None:
        write_png(out_png, (np.clip(img, 0, 1) * 255).astype(np.uint8))
        return

    V, N, UV, VC, tex_img = loaded
    # Normalize bounding box to unit sphere centered at origin
    vmin = V.reshape(-1, 3).min(axis=0)
    vmax = V.reshape(-1, 3).max(axis=0)
    center = 0.5 * (vmin + vmax)
    scale = max(float(np.max(vmax - vmin)), 1e-5)
    V = (V - center) / (scale * 0.58)

    # Rotate 3/4 view (yaw 35 deg, pitch 14 deg)
    yaw = math.radians(34.0)
    pitch = math.radians(14.0)
    cy, sy = math.cos(yaw), math.sin(yaw)
    cp, sp = math.cos(pitch), math.sin(pitch)
    Ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]], dtype=np.float32)
    Rx = np.array([[1, 0, 0], [0, cp, -sp], [0, sp, cp]], dtype=np.float32)
    R = Rx @ Ry
    Vr = V @ R.T

    # Recompute geometric face normals for crisp shading
    e1 = Vr[:, 1] - Vr[:, 0]
    e2 = Vr[:, 2] - Vr[:, 0]
    fn = np.cross(e1, e2)
    flen = np.linalg.norm(fn, axis=1, keepdims=True)
    fn = fn / np.maximum(flen, 1e-6)

    # Orthographic-ish gentle perspective projection
    z_cam = 3.2 - Vr[:, :, 2]
    persp = 2.6 / np.maximum(z_cam, 0.5)
    px = (Vr[:, :, 0] * persp * 0.45 + 0.5) * (size - 1)
    py = (0.5 - Vr[:, :, 1] * persp * 0.45) * (size - 1)
    pz = Vr[:, :, 2]

    # Triangle lighting (key light front-top-right + cool rim/fill)
    light_dir = np.array([0.45, 0.65, 0.62], dtype=np.float32)
    light_dir /= np.linalg.norm(light_dir)
    ndotl = np.abs(np.sum(fn * light_dir[None, :], axis=1))
    shade = 0.28 + 0.72 * ndotl

    # Per-triangle base color
    use_fallback = bool(entry.get("fallback_color")) and not bool(entry.get("use_vertex_color"))
    if use_fallback:
        # Convert linear fallback_color to sRGB for display
        fc = np.array(entry["fallback_color"], dtype=np.float32)
        srgb = np.power(np.clip(fc, 0.0, 1.0), 1.0 / 2.2)
        tri_col = np.tile(srgb, (len(V), 1))
    elif tex_img is not None and not bool(entry.get("use_vertex_color")):
        uv_c = UV.mean(axis=1)
        u = np.mod(uv_c[:, 0], 1.0)
        v = np.mod(uv_c[:, 1], 1.0)
        ti = np.clip((u * 255).astype(np.int32), 0, 255)
        tj = np.clip((v * 255).astype(np.int32), 0, 255)
        tri_col = tex_img[tj, ti]
    else:
        tri_col = np.clip(VC.mean(axis=1), 0.0, 1.0)

    lit_col = np.clip(tri_col * shade[:, None], 0.0, 1.0)

    # Painter's algorithm (depth-sorted back-to-front) + fast numpy bounding-box triangle fill
    tri_z = pz.mean(axis=1)
    order = np.argsort(tri_z)

    x0s, y0s = px[order, 0], py[order, 0]
    x1s, y1s = px[order, 1], py[order, 1]
    x2s, y2s = px[order, 2], py[order, 2]
    cols = lit_col[order]

    min_xs = np.clip(np.floor(np.minimum(np.minimum(x0s, x1s), x2s)).astype(np.int32), 0, size - 1)
    max_xs = np.clip(np.ceil(np.maximum(np.maximum(x0s, x1s), x2s)).astype(np.int32), 0, size - 1)
    min_ys = np.clip(np.floor(np.minimum(np.minimum(y0s, y1s), y2s)).astype(np.int32), 0, size - 1)
    max_ys = np.clip(np.ceil(np.maximum(np.maximum(y0s, y1s), y2s)).astype(np.int32), 0, size - 1)

    for i in range(len(order)):
        x0, y0, x1, y1, x2, y2 = x0s[i], y0s[i], x1s[i], y1s[i], x2s[i], y2s[i]
        area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
        if abs(area) < 0.05:
            continue
        x_lo, x_hi = min_xs[i], max_xs[i]
        y_lo, y_hi = min_ys[i], max_ys[i]
        if x_lo == x_hi and y_lo == y_hi:
            img[y_lo, x_lo] = cols[i]
            continue
        gx = np.arange(x_lo, x_hi + 1, dtype=np.float32)[None, :] + 0.5
        gy = np.arange(y_lo, y_hi + 1, dtype=np.float32)[:, None] + 0.5
        w0 = (x1 - x0) * (gy - y0) - (y1 - y0) * (gx - x0)
        w1 = (x2 - x1) * (gy - y1) - (y2 - y1) * (gx - x1)
        w2 = (x0 - x2) * (gy - y2) - (y0 - y2) * (gx - x2)
        if area > 0:
            mask = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        else:
            mask = (w0 <= 0) & (w1 <= 0) & (w2 <= 0)
        if mask.any():
            sub = img[y_lo : y_hi + 1, x_lo : x_hi + 1]
            sub[mask] = cols[i]

    write_png(out_png, (np.clip(img, 0, 1) * 255).astype(np.uint8))


def matches_word_keyword(lower_name: str, lower_kw: str) -> bool:
    """Exact Python mirror of ABFPVisualCatalog.cpp MatchesWordKeyword()."""
    if not lower_kw:
        return False
    nlen = len(lower_name)
    klen = len(lower_kw)
    start = 0
    while start <= nlen - klen:
        pos = lower_name.find(lower_kw, start)
        if pos == -1:
            return False
        left_ok = (pos == 0) or (not lower_name[pos - 1].isalpha())
        after = pos + klen
        right_ok = (after >= nlen) or (not lower_name[after].isalpha())
        if not right_ok and lower_name[after] == "s":
            right_ok = (after + 1 >= nlen) or (not lower_name[after + 1].isalpha())
        if left_ok and right_ok:
            return True
        start = pos + 1
    return False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--max-depth", type=int, default=3)
    parser.add_argument("--out-html", required=True)
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    raw_root = repo_root / "assets" / "raw"
    manifest = json.loads((repo_root / "assets" / "manifest.json").read_text(encoding="utf-8"))
    merge_fn = load_merger(repo_root)

    # Fetch Angband 4.2.6 monster.txt
    url = "https://raw.githubusercontent.com/angband/angband/f3082213b73f3e463e3d0d60bff4b00462beae6e/lib/gamedata/monster.txt"
    mtxt = urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "ABFP/1.0"}), timeout=20).read().decode("utf-8", errors="replace")
    monsters = []
    cur = {}
    for line in mtxt.splitlines():
        if line.startswith("name:"):
            if cur:
                monsters.append(cur)
            cur = {"name": line.split(":", 1)[1].strip(), "base": "", "depth": 999}
        elif line.startswith("base:"):
            cur["base"] = line.split(":", 1)[1].strip()
        elif line.startswith("depth:"):
            cur["depth"] = int(line.split(":", 1)[1].strip())
    if cur:
        monsters.append(cur)

    early = [m for m in monsters if m["name"] != "<player>" and m["depth"] <= args.max_depth]
    early.sort(key=lambda x: (x["depth"], x["name"].lower()))

    by_id = {c["id"]: c for c in manifest["creatures"]}
    default_id = "kaykit_rogue_hooded"

    # Match every monster using exact word-boundary matcher
    rows = []
    needed_ids = set()
    for m in early:
        lname = m["name"].lower()
        matched_id = None
        matched_kw = None
        for c in manifest["creatures"]:
            for kw in c.get("keywords", []):
                if matches_word_keyword(lname, kw.lower()):
                    matched_id = c["id"]
                    matched_kw = kw
                    break
            if matched_id:
                break
        if not matched_id:
            matched_id = default_id
            matched_kw = "(default)"
        needed_ids.add(matched_id)
        rows.append((m, matched_id, matched_kw))

    out_html = Path(args.out_html).resolve()
    thumb_dir = out_html.parent / "bestiary_thumbs"
    thumb_dir.mkdir(parents=True, exist_ok=True)

    # Render unique thumbnails for every matched creature model
    thumb_paths = {}
    for cid in sorted(needed_ids):
        c = by_id[cid]
        src = raw_root / c["dest"]
        render_src = src
        if c.get("merge_kaykit_body"):
            merged = src.with_name(f"{src.stem}_unified.glb")
            merge_fn(src, merged)
            render_src = merged
        png_path = thumb_dir / f"{cid}.png"
        render_mesh_thumbnail(render_src, png_path, c, size=144)
        thumb_paths[cid] = str(png_path)
        print(f"Rendered {cid:26s} -> {png_path.name}")

    # Group by depth and build clean theme-aware HTML sheet
    by_depth = {}
    for item in rows:
        d = item[0]["depth"]
        by_depth.setdefault(d, []).append(item)

    sections_html = []
    for d in sorted(by_depth.keys()):
        title = "Depth 0 — Town" if d == 0 else f"Depth {d} ({d * 50} ft)"
        cards = []
        for m, cid, kw in by_depth[d]:
            c = by_id[cid]
            h_cm = c.get("target_height_cm", 175)
            is_def = kw == "(default)"
            badge_cls = "badge warn" if is_def else "badge"
            cards.append(
                f'<div class="card">'
                f'<img src="{thumb_paths[cid]}" alt="{cid}" />'
                f'<div class="meta">'
                f'<div class="mname">{m["name"]}</div>'
                f'<div class="mbase">{m["base"]} · {h_cm}cm</div>'
                f'<div class="{badge_cls}">{cid} <span class="kw">({kw})</span></div>'
                f'</div></div>'
            )
        sections_html.append(
            f'<section><h3>{title} <span class="cnt">{len(by_depth[d])}</span></h3>'
            f'<div class="grid">{"".join(cards)}</div></section>'
        )

    html = f"""<!doctype html>
<html><head><meta charset="utf-8"><style>
body {{ margin: 0; font-family: var(--font-sans, system-ui, sans-serif); color: var(--foreground, #eaeaea); }}
.hdr {{ display: flex; justify-content: space-between; align-items: baseline; margin-bottom: 12px; padding-bottom: 8px; border-bottom: 1px solid var(--border, #333); }}
.hdr h2 {{ margin: 0; font-size: 16px; }}
.hdr .sub {{ font-size: 12px; color: var(--muted-foreground, #999); }}
section {{ margin-bottom: 18px; }}
h3 {{ margin: 0 0 8px 0; font-size: 13px; letter-spacing: 0.04em; text-transform: uppercase; color: var(--primary, #e5b567); display: flex; align-items: center; gap: 6px; }}
.cnt {{ font-size: 11px; padding: 1px 6px; border-radius: 999px; background: var(--secondary, #262933); color: var(--foreground, #ddd); }}
.grid {{ display: grid; grid-template-columns: repeat(auto-fill, minmax(156px, 1fr)); gap: 8px; }}
.card {{ background: var(--card, #181b22); border: 1px solid var(--border, #2b303c); border-radius: var(--radius, 8px); overflow: hidden; display: flex; flex-direction: column; }}
.card img {{ width: 100%; height: 112px; object-fit: contain; background: #141822; display: block; }}
.meta {{ padding: 6px 8px; display: flex; flex-direction: column; gap: 2px; }}
.mname {{ font-size: 12px; font-weight: 600; line-height: 1.25; }}
.mbase {{ font-size: 10.5px; color: var(--muted-foreground, #9aa0ad); }}
.badge {{ margin-top: 2px; font-size: 10px; font-family: var(--font-mono, monospace); color: var(--success, #68d391); white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }}
.badge.warn {{ color: var(--warning, #f6ad55); }}
.kw {{ opacity: 0.75; }}
</style></head><body>
<div class="hdr">
  <h2>Bestiary Visual Audit — Town through Depth {args.max_depth} ({args.max_depth * 50} ft)</h2>
  <div class="sub">{len(rows)} monsters · {len(needed_ids)} unique 3D models · word-boundary matching</div>
</div>
{"".join(sections_html)}
</body></html>"""

    out_html.write_text(html, encoding="utf-8")
    print(f"Wrote bestiary review sheet ({len(rows)} monsters, {len(needed_ids)} unique meshes) to {out_html}")


if __name__ == "__main__":
    main()
