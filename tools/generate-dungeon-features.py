#!/usr/bin/env python3
"""Generate bespoke textured, centered, watertight GLB dungeon architecture features
(StairsDown sunken stairwell & StairsUp rising stone staircase) for Angband: First Person.

Uses Python 3 standard library only (zero external dependencies).
Output GLBs follow Blender's exact UE 5.8 Interchange-verified glTF 2.0 binary layout
(uint16 indices, named scene/image, embedded PNG texture, glTF +Z = open entrance).
"""

import json
import math
import struct
import zlib
from pathlib import Path


def _png_chunk(chunk_type: bytes, data: bytes) -> bytes:
    crc = zlib.crc32(chunk_type + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + chunk_type + data + struct.pack(">I", crc)


def make_png(width: int, height: int, rgb_fn) -> bytes:
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        v = y / max(1, height - 1)
        for x in range(width):
            u = x / max(1, width - 1)
            r, g, b = rgb_fn(u, v)
            raw.append(max(0, min(255, int(r))))
            raw.append(max(0, min(255, int(g))))
            raw.append(max(0, min(255, int(b))))
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    idat = zlib.compress(bytes(raw), level=6)
    return (
        b"\x89PNG\r\n\x1a\n"
        + _png_chunk(b"IHDR", ihdr)
        + _png_chunk(b"IDAT", idat)
        + _png_chunk(b"IEND", b"")
    )


def _hash2(x: float, y: float) -> float:
    h = math.sin(x * 127.1 + y * 311.7) * 43758.5453123
    return h - math.floor(h)


def _value_noise(x: float, y: float) -> float:
    ix, iy = math.floor(x), math.floor(y)
    fx, fy = x - ix, y - iy
    ux = fx * fx * (3.0 - 2.0 * fx)
    uy = fy * fy * (3.0 - 2.0 * fy)
    a = _hash2(ix, iy)
    b = _hash2(ix + 1, iy)
    c = _hash2(ix, iy + 1)
    d = _hash2(ix + 1, iy + 1)
    return (a * (1 - ux) + b * ux) * (1 - uy) + (c * (1 - ux) + d * ux) * uy


def _fbm(x: float, y: float) -> float:
    return (
        0.55 * _value_noise(x, y)
        + 0.30 * _value_noise(x * 2.13 + 7.1, y * 2.13 + 3.4)
        + 0.15 * _value_noise(x * 4.7 + 1.9, y * 4.7 + 11.2)
    )


class ArchMeshBuilder:
    def __init__(self):
        self.pos = []
        self.nor = []
        self.uv = []
        self.idx = []

    def add_quad(self, p0, p1, p2, p3, u0=0.0, v0=0.0, u1=1.0, v1=1.0):
        """Add a CCW outward-facing quad (p0, p1, p2, p3) with flat face normal."""
        ax, ay, az = p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]
        bx, by, bz = p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]
        nx = ay * bz - az * by
        ny = az * bx - ax * bz
        nz = ax * by - ay * bx
        ln = math.sqrt(nx * nx + ny * ny + nz * nz)
        if ln < 1e-8:
            return
        n = (nx / ln, ny / ln, nz / ln)
        base = len(self.pos)
        self.pos.extend([p0, p1, p2, p3])
        self.nor.extend([n, n, n, n])
        self.uv.extend([(u0, v0), (u1, v0), (u1, v1), (u0, v1)])
        self.idx.extend([base, base + 1, base + 2, base, base + 2, base + 3])

    def add_box(self, x0, y0, z0, x1, y1, z1, uv_rect=(0.0, 0.0, 0.5, 0.5), faces="all"):
        """Add an axis-aligned box with outward normals mapped into uv_rect=(u0, v0, u1, v1)."""
        u0, v0, u1, v1 = uv_rect
        all_f = faces == "all"
        if all_f or "+y" in faces:
            self.add_quad((x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0), u0, v0, u1, v1)
        if all_f or "-y" in faces:
            self.add_quad((x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1), u0, v0, u1, v1)
        if all_f or "+z" in faces:
            self.add_quad((x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1), u0, v0, u1, v1)
        if all_f or "-z" in faces:
            self.add_quad((x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0), u0, v0, u1, v1)
        if all_f or "+x" in faces:
            self.add_quad((x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1), u0, v0, u1, v1)
        if all_f or "-x" in faces:
            self.add_quad((x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0), u0, v0, u1, v1)

    def write_glb(self, out_path: Path, png_bytes: bytes, roughness=0.84, metallic=0.05):
        pmin = [min(p[k] for p in self.pos) for k in range(3)]
        pmax = [max(p[k] for p in self.pos) for k in range(3)]

        buf = bytearray()

        def add_chunk(raw_bytes):
            while len(buf) % 4 != 0:
                buf.append(0)
            off = len(buf)
            buf.extend(raw_bytes)
            return off, len(raw_bytes)

        pos_raw = struct.pack(f"<{len(self.pos) * 3}f", *[v for p in self.pos for v in p])
        nor_raw = struct.pack(f"<{len(self.nor) * 3}f", *[v for n in self.nor for v in n])
        uv_raw = struct.pack(f"<{len(self.uv) * 2}f", *[v for t in self.uv for v in t])
        use_u16 = len(self.pos) < 65535
        idx_comp_type = 5123 if use_u16 else 5125
        idx_raw = struct.pack(f"<{len(self.idx)}{'H' if use_u16 else 'I'}", *self.idx)

        pos_off, pos_len = add_chunk(pos_raw)
        nor_off, nor_len = add_chunk(nor_raw)
        uv_off, uv_len = add_chunk(uv_raw)
        idx_off, idx_len = add_chunk(idx_raw)
        img_off, img_len = add_chunk(png_bytes)
        while len(buf) % 4 != 0:
            buf.append(0)

        gltf = {
            "asset": {"version": "2.0", "generator": "ABFP Dungeon Architecture"},
            "scene": 0,
            "scenes": [{"name": "Scene", "nodes": [0]}],
            "nodes": [{"name": out_path.stem, "mesh": 0}],
            "meshes": [
                {
                    "name": out_path.stem,
                    "primitives": [
                        {
                            "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
                            "indices": 3,
                            "material": 0,
                        }
                    ],
                }
            ],
            "materials": [
                {
                    "name": f"{out_path.stem}_Mat",
                    "pbrMetallicRoughness": {
                        "baseColorTexture": {"index": 0, "texCoord": 0},
                        "metallicFactor": metallic,
                        "roughnessFactor": roughness,
                    },
                }
            ],
            "textures": [{"source": 0}],
            "images": [{"bufferView": 4, "mimeType": "image/png", "name": f"{out_path.stem}_Tex"}],
            "bufferViews": [
                {"buffer": 0, "byteOffset": pos_off, "byteLength": pos_len, "target": 34962},
                {"buffer": 0, "byteOffset": nor_off, "byteLength": nor_len, "target": 34962},
                {"buffer": 0, "byteOffset": uv_off, "byteLength": uv_len, "target": 34962},
                {"buffer": 0, "byteOffset": idx_off, "byteLength": idx_len, "target": 34963},
                {"buffer": 0, "byteOffset": img_off, "byteLength": img_len},
            ],
            "accessors": [
                {"bufferView": 0, "componentType": 5126, "count": len(self.pos), "type": "VEC3", "min": pmin, "max": pmax},
                {"bufferView": 1, "componentType": 5126, "count": len(self.nor), "type": "VEC3"},
                {"bufferView": 2, "componentType": 5126, "count": len(self.uv), "type": "VEC2"},
                {"bufferView": 3, "componentType": idx_comp_type, "count": len(self.idx), "type": "SCALAR", "min": [0], "max": [max(self.idx)]},
            ],
            "buffers": [{"byteLength": len(buf)}],
        }

        jbytes = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
        while len(jbytes) % 4 != 0:
            jbytes += b" "

        total_len = 12 + 8 + len(jbytes) + 8 + len(buf)
        header = struct.pack("<III", 0x46546C67, 2, total_len)
        jchunk = struct.pack("<II", len(jbytes), 0x4E4F534A) + jbytes
        bchunk = struct.pack("<II", len(buf), 0x004E4942) + bytes(buf)

        out_path.parent.mkdir(parents=True, exist_ok=True)
        out_path.write_bytes(header + jchunk + bchunk)


def _stair_atlas_rgb(u: float, v: float):
    """4-quadrant weathered dungeon stone + iron + abyss texture atlas:
    - Top-Left  (u<0.5, v<0.5): Warm sandstone/ashlar stone blocks (matches 2K Poly Haven walls)
    - Top-Right (u>=0.5, v<0.5): Worn flagstone / step tread stone with mossy crevices
    - Bot-Left  (u<0.5, v>=0.5): Deep shadowed pit stone / dark descending stairwell stone
    - Bot-Right (u>=0.5, v>=0.5): Dark rusted iron + brass lantern cap band (for corner posts)
    """
    if u < 0.5 and v < 0.5:
        lu, lv = u * 2.0, v * 2.0
        row = int(lv * 6.0)
        off = 0.5 if (row % 2) else 0.0
        bx = (lu * 3.0 + off) % 1.0
        by = (lv * 6.0) % 1.0
        seam = 1.0 if (bx < 0.055 or bx > 0.945 or by < 0.075 or by > 0.925) else 0.0
        block_var = (_hash2(math.floor(lu * 3.0 + off), row) - 0.5) * 28.0
        n = _fbm(lu * 10.0, lv * 10.0)
        r = 128 + block_var + (n - 0.5) * 40
        g = 109 + block_var * 0.85 + (n - 0.5) * 34
        b = 84 + block_var * 0.70 + (n - 0.5) * 26
        if seam:
            r, g, b = r * 0.42, g * 0.42, b * 0.44
        return (r, g, b)

    if u >= 0.5 and v < 0.5:
        lu, lv = (u - 0.5) * 2.0, v * 2.0
        n = _fbm(lu * 12.0 + 3.0, lv * 12.0 + 5.0)
        bx = (lu * 4.0) % 1.0
        by = (lv * 4.0) % 1.0
        seam = 1.0 if (bx < 0.04 or by < 0.04) else 0.0
        wear = math.sin(lu * math.pi) * 16.0
        moss = max(0.0, _fbm(lu * 7.0 + 19.0, lv * 7.0) - 0.64) * 140.0
        r = 108 + wear + (n - 0.5) * 34 - moss * 0.35
        g = 99 + wear * 0.92 + (n - 0.5) * 30 + moss * 0.25
        b = 86 + wear * 0.82 + (n - 0.5) * 26 - moss * 0.45
        if seam:
            r, g, b = r * 0.48, g * 0.48, b * 0.50
        return (r, g, b)

    if u < 0.5 and v >= 0.5:
        lu, lv = u * 2.0, (v - 0.5) * 2.0
        # Vertical depth gradient: lv=0 near top, lv=1 deep abyss
        depth = max(0.0, min(1.0, lv))
        n = _fbm(lu * 11.0 + 9.0, lv * 11.0 + 2.0)
        bx = (lu * 4.0 + (0.5 if int(lv * 6.0) % 2 else 0.0)) % 1.0
        by = (lv * 6.0) % 1.0
        seam = 1.0 if (bx < 0.05 or by < 0.06) else 0.0
        fade = (1.0 - depth * 0.86)
        r = (76 + (n - 0.5) * 26) * fade + 4
        g = (72 + (n - 0.5) * 24) * fade + 6
        b = (68 + (n - 0.5) * 22) * fade + 11
        if seam:
            r, g, b = r * 0.45, g * 0.45, b * 0.52
        return (r, g, b)

    # Bot-right: Dark forged iron (v < 0.86) + warm glowing bronze/amber finial (v >= 0.86)
    lu, lv = (u - 0.5) * 2.0, (v - 0.5) * 2.0
    n = _fbm(lu * 14.0, lv * 14.0)
    if lv > 0.72:
        # Warm brass/amber brazier finial accent
        glow = 0.85 + 0.15 * math.sin(lu * math.pi)
        return (215 * glow + (n - 0.5) * 22, 152 * glow + (n - 0.5) * 18, 62 * glow)
    rust = max(0.0, _fbm(lu * 8.0 + 4.0, lv * 8.0 + 9.0) - 0.52) * 120.0
    return (38 + rust * 0.8 + (n - 0.5) * 16, 36 + rust * 0.35 + (n - 0.5) * 14, 38 + (n - 0.5) * 14)


UV_ASHLAR = (0.02, 0.02, 0.48, 0.48)
UV_TREAD = (0.52, 0.02, 0.98, 0.48)
UV_PIT_TOP = (0.02, 0.52, 0.48, 0.72)
UV_PIT_DEEP = (0.02, 0.52, 0.48, 0.98)
UV_ABYSS = (0.04, 0.90, 0.46, 0.98)
UV_IRON = (0.52, 0.52, 0.98, 0.82)
UV_BRASS = (0.54, 0.88, 0.96, 0.98)


def generate_stairs_down(out_path: Path, png_bytes: bytes):
    """Watertight 4.0x4.0m sunken stone dungeon stairwell descending down into the depths.
    Entrance is on glTF +Z (UE +X at Yaw=0), steps descend toward -Z."""
    mb = ArchMeshBuilder()

    # 1. Watertight 4.0x4.0m outer floor collar flush at Y=0.0 (thickness 0.14m below Y=0)
    # Surrounding central pit opening X=[-1.15, +1.15], Z=[-1.35, +1.50]
    pit_x0, pit_x1 = -1.15, 1.15
    pit_z0, pit_z1 = -1.35, 1.50
    collar_bot = -0.14
    mb.add_box(-2.0, collar_bot, -2.0, pit_x0, 0.0, 2.0, UV_TREAD)       # West flange
    mb.add_box(pit_x1, collar_bot, -2.0, 2.0, 0.0, 2.0, UV_TREAD)        # East flange
    mb.add_box(pit_x0, collar_bot, -2.0, pit_x1, 0.0, pit_z0, UV_TREAD)  # Back (-Z) flange
    mb.add_box(pit_x0, collar_bot, pit_z1, pit_x1, 0.0, 2.0, UV_TREAD)   # Front (+Z) threshold

    # 2. Raised carved stone parapet curbs around Left, Right, and Back (cleanly abutting bollards — zero overlap)
    mb.add_box(-1.50, 0.002, -1.34, -1.15, 0.76, 1.30, UV_ASHLAR)        # Left parapet wall
    mb.add_box(-1.55, 0.762, -1.34, -1.11, 0.88, 1.30, UV_TREAD)         # Left coping cap
    mb.add_box(1.15, 0.002, -1.34, 1.50, 0.76, 1.30, UV_ASHLAR)          # Right parapet wall
    mb.add_box(1.11, 0.762, -1.34, 1.55, 0.88, 1.30, UV_TREAD)           # Right coping cap
    mb.add_box(-1.14, 0.002, -1.68, 1.14, 0.88, -1.35, UV_ASHLAR)        # Rear parapet wall
    mb.add_box(-1.14, 0.882, -1.72, 1.14, 1.00, -1.31, UV_TREAD)         # Rear coping cap

    # 3. Four stone-and-iron entrance/corner bollards with warm bronze finials so stairs read clearly from afar
    for px in (-1.33, 1.33):
        for pz, top_h in ((1.48, 1.24), (-1.52, 1.36)):
            mb.add_box(px - 0.18, 0.002, pz - 0.18, px + 0.18, top_h, pz + 0.18, UV_ASHLAR)
            mb.add_box(px - 0.22, top_h + 0.002, pz - 0.22, px + 0.22, top_h + 0.09, pz + 0.22, UV_IRON)
            mb.add_box(px - 0.11, top_h + 0.092, pz - 0.11, px + 0.11, top_h + 0.25, pz + 0.11, UV_BRASS)

    # 4. Iron handrails flanking the entrance descent
    for px in (-1.12, 1.12):
        mb.add_box(px - 0.035, 0.96, -1.33, px + 0.035, 1.03, 1.29, UV_IRON)

    # 5. Enclosed deep sunken stone pit walls (Y = -2.65 .. 0.0) so void is never visible
    pit_bot = -2.65
    mb.add_quad((pit_x0, pit_bot, pit_z1), (pit_x0, pit_bot, pit_z0), (pit_x0, 0.0, pit_z0), (pit_x0, 0.0, pit_z1), *UV_PIT_DEEP)
    mb.add_quad((pit_x1, pit_bot, pit_z0), (pit_x1, pit_bot, pit_z1), (pit_x1, 0.0, pit_z1), (pit_x1, 0.0, pit_z0), *UV_PIT_DEEP)
    mb.add_quad((pit_x0, pit_bot, pit_z0), (pit_x1, pit_bot, pit_z0), (pit_x1, 0.0, pit_z0), (pit_x0, 0.0, pit_z0), *UV_PIT_DEEP)
    mb.add_quad((pit_x1, pit_bot, pit_z1), (pit_x0, pit_bot, pit_z1), (pit_x0, 0.0, pit_z1), (pit_x1, 0.0, pit_z1), *UV_PIT_DEEP)
    # Abyssal bottom floor
    mb.add_quad((pit_x0, pit_bot, pit_z1), (pit_x1, pit_bot, pit_z1), (pit_x1, pit_bot, pit_z0), (pit_x0, pit_bot, pit_z0), *UV_ABYSS)

    # 6. 9 wide carved stone steps descending cleanly from +Z (threshold) down toward -Z (dungeon depths)
    n_steps = 9
    step_depth = (pit_z1 - pit_z0 - 0.35) / n_steps
    step_drop = 0.255
    for i in range(n_steps):
        sz1 = pit_z1 - i * step_depth
        sz0 = sz1 - step_depth
        sy1 = -i * step_drop - 0.06
        sy0 = sy1 - 0.26
        if i < 4:
            step_uv = UV_TREAD
        else:
            frac = (i - 3) / float(n_steps - 3)
            v_lo = 0.54 + frac * 0.30
            step_uv = (0.03, v_lo, 0.47, min(0.97, v_lo + 0.14))
        mb.add_box(pit_x0 + 0.015, sy0, sz0, pit_x1 - 0.015, sy1, sz1, step_uv)

    # 7. Dark lower archway frame at bottom back of pit hinting at the subterranean corridor below
    mb.add_box(-1.14, pit_bot, pit_z0 + 0.01, -0.78, -0.55, pit_z0 + 0.28, UV_PIT_DEEP)
    mb.add_box(0.78, pit_bot, pit_z0 + 0.01, 1.14, -0.55, pit_z0 + 0.28, UV_PIT_DEEP)
    mb.add_box(-1.14, -0.82, pit_z0 + 0.01, 1.14, -0.46, pit_z0 + 0.32, UV_PIT_DEEP)
    mb.add_quad((-0.78, pit_bot, pit_z0 + 0.03), (0.78, pit_bot, pit_z0 + 0.03), (0.78, -0.82, pit_z0 + 0.03), (-0.78, -0.82, pit_z0 + 0.03), *UV_ABYSS)

    mb.write_glb(out_path, png_bytes, roughness=0.84, metallic=0.08)


def generate_stairs_up(out_path: Path, png_bytes: bytes):
    """Centered carved stone dungeon ascent staircase (3.1x3.3m, rising Y=0..+2.85m).
    Entrance is on glTF +Z (UE +X at Yaw=0), steps rise toward -Z."""
    mb = ArchMeshBuilder()

    # 1. 10 solid stone steps rising from +Z (+1.55) to -Z (-1.45)
    n_steps = 10
    z_start = 1.55
    z_end = -1.45
    step_d = (z_start - z_end) / n_steps
    step_h = 2.65 / n_steps
    for i in range(n_steps):
        sz1 = z_start - i * step_d
        sz0 = z_end
        sy1 = (i + 1) * step_h
        sy0 = i * step_h + 0.002
        mb.add_box(-1.17, sy0, sz0, 1.17, sy1, sz1, UV_TREAD)
        mb.add_box(-1.17, sy1 - 0.055, sz1, 1.17, sy1, sz1 + 0.035, UV_ASHLAR)

    # 2. Stepped flanking ashlar stone side walls/balustrades (non-overlapping segments)
    seg_span = (1.36 - (-1.32)) / 5.0
    for sx0, sx1 in ((-1.52, -1.18), (1.18, 1.52)):
        for seg in range(5):
            seg_z1 = 1.36 - seg * seg_span
            seg_z0 = seg_z1 - seg_span + 0.004
            wall_h = 0.85 + seg * 0.52
            mb.add_box(sx0, 0.0, seg_z0, sx1, wall_h, seg_z1, UV_ASHLAR)
            mb.add_box(sx0 - 0.04, wall_h + 0.002, seg_z0, sx1 + 0.04, wall_h + 0.09, seg_z1, UV_TREAD)

    # 3. Front newel posts with iron caps & brass finials
    for px in (-1.35, 1.35):
        mb.add_box(px - 0.19, 0.0, z_start - 0.18, px + 0.19, 1.24, z_start + 0.18, UV_ASHLAR)
        mb.add_box(px - 0.23, 1.242, z_start - 0.22, px + 0.23, 1.33, z_start + 0.22, UV_IRON)
        mb.add_box(px - 0.11, 1.332, z_start - 0.11, px + 0.11, 1.48, z_start + 0.11, UV_BRASS)

    # 4. Rear top archway pillars & lintel framing ascent to upper level
    mb.add_box(-1.54, 0.0, z_end - 0.24, -1.15, 2.86, z_end + 0.12, UV_ASHLAR)
    mb.add_box(1.15, 0.0, z_end - 0.24, 1.54, 2.86, z_end + 0.12, UV_ASHLAR)
    mb.add_box(-1.58, 2.862, z_end - 0.28, 1.58, 3.18, z_end + 0.16, UV_TREAD)

    mb.write_glb(out_path, png_bytes, roughness=0.84, metallic=0.06)


def generate_all(raw_root: Path) -> int:
    terrain_dir = raw_root / "terrain"
    terrain_dir.mkdir(parents=True, exist_ok=True)
    atlas_png = make_png(256, 256, _stair_atlas_rgb)
    generate_stairs_down(terrain_dir / "abfp_stairs_down.glb", atlas_png)
    generate_stairs_up(terrain_dir / "abfp_stairs_up.glb", atlas_png)
    return 2


if __name__ == "__main__":
    repo = Path(__file__).resolve().parent.parent
    n = generate_all(repo / "assets" / "raw")
    print(f"Generated {n} bespoke dungeon architecture GLBs.")
