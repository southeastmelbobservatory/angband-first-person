#!/usr/bin/env python3
"""Generate 19 bespoke textured medieval 3D item & viewmodel GLBs covering every
Angband 4.2.6 object category (TV_SHOT through TV_MUSHROOM) plus first-person
handheld weapons and light sources (torch, phial, mace, spear, pickaxe).

Uses Python 3 standard library only (zero external dependencies).
Output GLBs follow Blender's exact UE 5.8 Interchange-verified glTF 2.0 binary layout
(uint16 indices, named scene/image, embedded 256x256 PNG texture).
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


def _noise(x: float, y: float) -> float:
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
    return 0.6 * _noise(x, y) + 0.3 * _noise(x * 2.2 + 5.1, y * 2.2 + 3.7) + 0.1 * _noise(x * 4.5 + 1.3, y * 4.5 + 9.2)


# 4x4 = 16-swatch procedural medieval material atlas (512x512) shared cleanly across all bespoke items
def _item_atlas_rgb(u: float, v: float):
    col = min(3, int(u * 4.0))
    row = min(3, int(v * 4.0))
    lu = (u * 4.0) - col
    lv = (v * 4.0) - row
    n = _fbm(lu * 10.0 + col * 7.3, lv * 10.0 + row * 11.1)
    d = (n - 0.5) * 28.0

    idx = row * 4 + col
    # Row 0: Woods & Leathers
    if idx == 0:  # Dark oak / ash wood grain
        grain = math.sin((lu * 18.0 + n * 2.5) * math.pi) * 12.0
        return (86 + d * 0.7 + grain, 55 + d * 0.5 + grain * 0.7, 30 + d * 0.4 + grain * 0.4)
    if idx == 1:  # Warm yew / light wood
        grain = math.sin((lu * 16.0 + n * 2.0) * math.pi) * 14.0
        return (148 + d * 0.7 + grain, 98 + d * 0.5 + grain * 0.7, 50 + d * 0.4)
    if idx == 2:  # Tanned saddle leather
        return (112 + d * 0.9, 64 + d * 0.6, 32 + d * 0.4)
    if idx == 3:  # Dark weathered boot/strap leather
        return (56 + d * 0.7, 36 + d * 0.5, 24 + d * 0.4)

    # Row 1: Metals
    if idx == 4:  # Forged steel / armor plate
        brush = (n - 0.5) * 32.0
        return (162 + brush, 168 + brush, 178 + brush)
    if idx == 5:  # Dark iron / bindings
        return (66 + d * 0.8, 68 + d * 0.8, 74 + d * 0.8)
    if idx == 6:  # Burnished gold / brass
        return (218 + d * 0.7, 174 + d * 0.6, 58 + d * 0.4)
    if idx == 7:  # Gleaming silver / mithril
        return (206 + d * 0.6, 218 + d * 0.6, 232 + d * 0.5)

    # Row 2: Fabrics & Scales
    if idx == 8:  # Forest ranger cloak cloth
        weave = ((int(lu * 32) + int(lv * 32)) % 2) * 8.0
        return (38 + d * 0.7 + weave, 78 + d * 0.8 + weave, 46 + d * 0.6 + weave)
    if idx == 9:  # Crimson cloth / ruby / mushroom cap
        spots = 1.0 if ((lu - 0.3) ** 2 + (lv - 0.35) ** 2 < 0.015 or (lu - 0.7) ** 2 + (lv - 0.65) ** 2 < 0.018 or (lu - 0.65) ** 2 + (lv - 0.25) ** 2 < 0.012) else 0.0
        if spots:
            return (235, 224, 198)
        return (178 + d * 0.9, 34 + d * 0.4, 28 + d * 0.4)
    if idx == 10:  # Dragon scale crimson-bronze
        sx = (lu * 6.0 + (0.5 if int(lv * 6.0) % 2 else 0.0)) % 1.0
        sy = (lv * 6.0) % 1.0
        edge = 1.0 if ((sx - 0.5) ** 2 + (sy - 0.2) ** 2 > 0.38) else 0.0
        sc = 0.65 if edge else 1.0
        return ((152 + d) * sc, (34 + d * 0.4) * sc, (26 + d * 0.3) * sc)
    if idx == 11:  # Cream mushroom stem / parchment / hemp string
        return (214 + d * 0.7, 198 + d * 0.6, 162 + d * 0.5)

    # Row 3: Gems, Fire & Arcane Glow
    if idx == 12:  # Blazing torch flame (yellow-orange gradient)
        return (255, 165 + lv * 75, 25 + lv * 95)
    if idx == 13:  # Starlight / sapphire arcane crystal
        return (105 + d * 0.7, 185 + d * 0.6, 252)
    if idx == 14:  # Emerald gem
        return (36 + d * 0.6, 192 + d * 0.8, 92 + d * 0.6)
    # idx == 15: Ruby gem / charred rag
    return (210 + d * 0.7, 36 + d * 0.4, 48 + d * 0.4)


def uv_cell(idx: int):
    col = idx % 4
    row = idx // 4
    pad = 0.025
    return (col * 0.25 + pad, row * 0.25 + pad, (col + 1) * 0.25 - pad, (row + 1) * 0.25 - pad)


UV_OAK = uv_cell(0)
UV_YEW = uv_cell(1)
UV_LEATHER = uv_cell(2)
UV_DARK_LEATHER = uv_cell(3)
UV_STEEL = uv_cell(4)
UV_IRON = uv_cell(5)
UV_GOLD = uv_cell(6)
UV_MITHRIL = uv_cell(7)
UV_CLOAK = uv_cell(8)
UV_MUSH_CAP = uv_cell(9)
UV_DRAGON = uv_cell(10)
UV_CREAM = uv_cell(11)
UV_FLAME = uv_cell(12)
UV_STARLIGHT = uv_cell(13)
UV_EMERALD = uv_cell(14)
UV_RUBY = uv_cell(15)


class ItemMeshBuilder:
    def __init__(self):
        self.pos = []
        self.nor = []
        self.uv = []
        self.idx = []

    def add_tri(self, p0, p1, p2, uv0, uv1, uv2):
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
        self.pos.extend([p0, p1, p2])
        self.nor.extend([n, n, n])
        self.uv.extend([uv0, uv1, uv2])
        self.idx.extend([base, base + 1, base + 2])

    def add_quad(self, p0, p1, p2, p3, uv_rect):
        u0, v0, u1, v1 = uv_rect
        self.add_tri(p0, p1, p2, (u0, v0), (u1, v0), (u1, v1))
        self.add_tri(p0, p2, p3, (u0, v0), (u1, v1), (u0, v1))

    def add_box(self, x0, y0, z0, x1, y1, z1, uv_rect):
        self.add_quad((x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0), uv_rect)
        self.add_quad((x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1), uv_rect)
        self.add_quad((x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1), uv_rect)
        self.add_quad((x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0), uv_rect)
        self.add_quad((x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1), uv_rect)
        self.add_quad((x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0), uv_rect)

    def add_frustum_y(self, cx, cz, y0, y1, r0, r1, uv_rect, segs=14, cap_bot=True, cap_top=True):
        u0, v0, u1, v1 = uv_rect
        um, vm = 0.5 * (u0 + u1), 0.5 * (v0 + v1)
        b_ring = []
        t_ring = []
        for s in range(segs):
            a = 2.0 * math.pi * s / segs
            ca, sa = math.cos(a), math.sin(a)
            b_ring.append((cx + ca * r0, y0, cz + sa * r0))
            t_ring.append((cx + ca * r1, y1, cz + sa * r1))
        for s in range(segs):
            ns = (s + 1) % segs
            if r0 > 1e-5 and r1 > 1e-5:
                self.add_quad(b_ring[ns], b_ring[s], t_ring[s], t_ring[ns], uv_rect)
            elif r1 <= 1e-5:
                self.add_tri(b_ring[ns], b_ring[s], (cx, y1, cz), (u0, v0), (u1, v0), (um, v1))
            else:
                self.add_tri((cx, y0, cz), t_ring[s], t_ring[ns], (um, v0), (u0, v1), (u1, v1))
        if cap_bot and r0 > 1e-5:
            cb = (cx, y0, cz)
            for s in range(segs):
                ns = (s + 1) % segs
                self.add_tri(cb, b_ring[s], b_ring[ns], (um, vm), (u1, v0), (u1, v1))
        if cap_top and r1 > 1e-5:
            ct = (cx, y1, cz)
            for s in range(segs):
                ns = (s + 1) % segs
                self.add_tri(ct, t_ring[ns], t_ring[s], (um, vm), (u1, v0), (u1, v1))

    def add_sphere(self, cx, cy, cz, rx, ry, rz, uv_rect, rings=8, segs=14):
        u0, v0, u1, v1 = uv_rect
        pts = []
        for r in range(rings + 1):
            phi = math.pi * r / rings
            sp, cp = math.sin(phi), math.cos(phi)
            row = []
            for s in range(segs):
                th = 2.0 * math.pi * s / segs
                row.append((cx + rx * sp * math.cos(th), cy + ry * cp, cz + rz * sp * math.sin(th)))
            pts.append(row)
        for r in range(rings):
            for s in range(segs):
                ns = (s + 1) % segs
                p00, p01 = pts[r][s], pts[r][ns]
                p10, p11 = pts[r + 1][s], pts[r + 1][ns]
                if r > 0:
                    self.add_tri(p00, p01, p10, (u0, v0), (u1, v0), (u0, v1))
                if r < rings - 1:
                    self.add_tri(p01, p11, p10, (u1, v0), (u1, v1), (u0, v1))

    def add_torus(self, cx, cy, cz, major_r, minor_r, uv_rect, major_segs=20, minor_segs=10, tilt_x=0.0):
        ct, st = math.cos(tilt_x), math.sin(tilt_x)

        def xform(x, y, z):
            ry = y * ct - z * st
            rz = y * st + z * ct
            return (cx + x, cy + ry, cz + rz)

        grid = []
        for i in range(major_segs):
            a = 2.0 * math.pi * i / major_segs
            ca, sa = math.cos(a), math.sin(a)
            ring = []
            for j in range(minor_segs):
                b = 2.0 * math.pi * j / minor_segs
                cb, sb = math.cos(b), math.sin(b)
                rr = major_r + minor_r * cb
                ring.append(xform(rr * ca, minor_r * sb, rr * sa))
            grid.append(ring)
        for i in range(major_segs):
            ni = (i + 1) % major_segs
            for j in range(minor_segs):
                nj = (j + 1) % minor_segs
                self.add_quad(grid[i][j], grid[ni][j], grid[ni][nj], grid[i][nj], uv_rect)

    def add_tube(self, points, radii, uv_rect, segs=10):
        """Swept circular tube along a 3D polyline."""
        rings = []
        n = len(points)
        for i, p in enumerate(points):
            if i == 0:
                tx, ty, tz = points[1][0] - p[0], points[1][1] - p[1], points[1][2] - p[2]
            elif i == n - 1:
                tx, ty, tz = p[0] - points[i - 1][0], p[1] - points[i - 1][1], p[2] - points[i - 1][2]
            else:
                tx, ty, tz = points[i + 1][0] - points[i - 1][0], points[i + 1][1] - points[i - 1][1], points[i + 1][2] - points[i - 1][2]
            tl = max(1e-6, math.sqrt(tx * tx + ty * ty + tz * tz))
            tx, ty, tz = tx / tl, ty / tl, tz / tl
            up = (0.0, 0.0, 1.0) if abs(tz) < 0.85 else (1.0, 0.0, 0.0)
            ux = ty * up[2] - tz * up[1]
            uy = tz * up[0] - tx * up[2]
            uz = tx * up[1] - ty * up[0]
            ul = max(1e-6, math.sqrt(ux * ux + uy * uy + uz * uz))
            ux, uy, uz = ux / ul, uy / ul, uz / ul
            vx = ty * uz - tz * uy
            vy = tz * ux - tx * uz
            vz = tx * uy - ty * ux
            rad = radii[i]
            ring = []
            for s in range(segs):
                a = 2.0 * math.pi * s / segs
                ca, sa = math.cos(a), math.sin(a)
                ring.append((
                    p[0] + rad * (ca * ux + sa * vx),
                    p[1] + rad * (ca * uy + sa * vy),
                    p[2] + rad * (ca * uz + sa * vz),
                ))
            rings.append(ring)
        for i in range(n - 1):
            for s in range(segs):
                ns = (s + 1) % segs
                self.add_quad(rings[i][s], rings[i + 1][s], rings[i + 1][ns], rings[i][ns], uv_rect)

    def write_glb(self, out_path: Path, png_bytes: bytes, roughness=0.55, metallic=0.25):
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
        idx_comp = 5123 if use_u16 else 5125
        idx_raw = struct.pack(f"<{len(self.idx)}{'H' if use_u16 else 'I'}", *self.idx)

        pos_off, pos_len = add_chunk(pos_raw)
        nor_off, nor_len = add_chunk(nor_raw)
        uv_off, uv_len = add_chunk(uv_raw)
        idx_off, idx_len = add_chunk(idx_raw)
        img_off, img_len = add_chunk(png_bytes)
        while len(buf) % 4 != 0:
            buf.append(0)

        gltf = {
            "asset": {"version": "2.0", "generator": "ABFP Item Armory"},
            "scene": 0,
            "scenes": [{"name": "Scene", "nodes": [0]}],
            "nodes": [{"name": out_path.stem, "mesh": 0}],
            "meshes": [{
                "name": out_path.stem,
                "primitives": [{
                    "attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
                    "indices": 3,
                    "material": 0,
                }],
            }],
            "materials": [{
                "name": f"{out_path.stem}_Mat",
                "pbrMetallicRoughness": {
                    "baseColorTexture": {"index": 0, "texCoord": 0},
                    "metallicFactor": metallic,
                    "roughnessFactor": roughness,
                },
            }],
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
                {"bufferView": 3, "componentType": idx_comp, "count": len(self.idx), "type": "SCALAR", "min": [0], "max": [max(self.idx)]},
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


def build_torch_hand(out_path: Path, png: bytes):
    """Handheld medieval wooden torch (upright +Y, handle at Y=0..0.34, brazier & ember tip at top)."""
    mb = ItemMeshBuilder()
    mb.add_frustum_y(0, 0, 0.0, 0.30, 0.018, 0.026, UV_OAK, segs=12)
    mb.add_frustum_y(0, 0, 0.04, 0.14, 0.022, 0.024, UV_DARK_LEATHER, segs=12)
    mb.add_frustum_y(0, 0, 0.28, 0.34, 0.028, 0.046, UV_IRON, segs=12)
    # Iron prongs cradling tar-soaked torch head
    for s in range(4):
        a = s * math.pi * 0.5 + 0.4
        px, pz = math.cos(a) * 0.044, math.sin(a) * 0.044
        mb.add_box(px - 0.008, 0.32, pz - 0.008, px + 0.008, 0.41, pz + 0.008, UV_IRON)
    mb.add_frustum_y(0, 0, 0.34, 0.41, 0.040, 0.046, UV_DARK_LEATHER, segs=12)
    mb.add_frustum_y(0, 0, 0.40, 0.51, 0.042, 0.002, UV_FLAME, segs=12, cap_top=False)
    mb.write_glb(out_path, png, roughness=0.68, metallic=0.18)


def build_phial(out_path: Path, png: bytes):
    """Radiant crystal Phial / Starlight artifact light."""
    mb = ItemMeshBuilder()
    mb.add_frustum_y(0, 0, 0.0, 0.04, 0.055, 0.045, UV_MITHRIL, segs=12)
    mb.add_sphere(0, 0.13, 0, 0.078, 0.095, 0.078, UV_STARLIGHT, rings=8, segs=16)
    mb.add_torus(0, 0.13, 0, 0.082, 0.009, UV_GOLD, major_segs=18, minor_segs=8)
    mb.add_frustum_y(0, 0, 0.21, 0.27, 0.028, 0.036, UV_MITHRIL, segs=12)
    mb.add_sphere(0, 0.28, 0, 0.026, 0.024, 0.026, UV_GOLD, rings=6, segs=10)
    mb.write_glb(out_path, png, roughness=0.20, metallic=0.35)


def build_mace(out_path: Path, png: bytes):
    """Flanged forged steel medieval mace / warhammer (upright +Y, grip near Y=0)."""
    mb = ItemMeshBuilder()
    mb.add_sphere(0, 0.02, 0, 0.034, 0.030, 0.034, UV_GOLD, rings=6, segs=10)
    mb.add_frustum_y(0, 0, 0.03, 0.24, 0.020, 0.022, UV_DARK_LEATHER, segs=12)
    mb.add_frustum_y(0, 0, 0.24, 0.60, 0.019, 0.024, UV_OAK, segs=12)
    mb.add_frustum_y(0, 0, 0.56, 0.78, 0.036, 0.040, UV_STEEL, segs=12)
    # 6 heavy steel flanges around mace head
    for s in range(6):
        a = s * math.pi / 3.0
        ca, sa = math.cos(a), math.sin(a)
        px, pz = ca * 0.058, sa * 0.058
        mb.add_box(px - 0.018, 0.58, pz - 0.018, px + 0.018, 0.77, pz + 0.018, UV_STEEL)
    mb.add_frustum_y(0, 0, 0.78, 0.87, 0.034, 0.002, UV_STEEL, segs=8, cap_top=False)
    mb.write_glb(out_path, png, roughness=0.36, metallic=0.72)


def build_spear(out_path: Path, png: bytes):
    """Forged leaf-blade dungeon spear / halberd (upright +Y)."""
    mb = ItemMeshBuilder()
    mb.add_frustum_y(0, 0, 0.0, 0.07, 0.014, 0.020, UV_IRON, segs=10)
    mb.add_frustum_y(0, 0, 0.07, 0.92, 0.020, 0.020, UV_OAK, segs=12)
    mb.add_frustum_y(0, 0, 0.86, 0.96, 0.026, 0.028, UV_GOLD, segs=12)
    # Broad steel leaf spearhead
    mb.add_box(-0.048, 0.95, -0.014, 0.048, 1.08, 0.014, UV_STEEL)
    mb.add_frustum_y(0, 0, 1.08, 1.28, 0.048, 0.002, UV_STEEL, segs=4, cap_top=False)
    # Side halberd fluke / lug
    mb.add_box(-0.088, 0.94, -0.010, 0.088, 0.98, 0.010, UV_IRON)
    mb.write_glb(out_path, png, roughness=0.38, metallic=0.65)


def build_pickaxe(out_path: Path, png: bytes):
    """Heavy iron miner's pickaxe / digging tool (upright +Y)."""
    mb = ItemMeshBuilder()
    mb.add_frustum_y(0, 0, 0.0, 0.76, 0.022, 0.025, UV_OAK, segs=10)
    mb.add_box(-0.045, 0.66, -0.045, 0.045, 0.76, 0.045, UV_IRON)
    # Curved double pick arms along X
    pts = [(-0.32, 0.60, 0.0), (-0.18, 0.70, 0.0), (0.0, 0.73, 0.0), (0.18, 0.70, 0.0), (0.32, 0.60, 0.0)]
    rads = [0.006, 0.024, 0.036, 0.024, 0.006]
    mb.add_tube(pts, rads, UV_STEEL, segs=8)
    mb.write_glb(out_path, png, roughness=0.45, metallic=0.68)


def build_bow(out_path: Path, png: bytes):
    """Curved laminated yew longbow + taut hemp bowstring."""
    mb = ItemMeshBuilder()
    pts = []
    rads = []
    for i in range(13):
        t = (i / 12.0) * 2.0 - 1.0  # -1..+1
        x = t * 0.52
        z = -0.18 * (1.0 - t * t)
        pts.append((x, 0.05, z))
        rads.append(0.022 - 0.011 * abs(t))
    mb.add_tube(pts, rads, UV_YEW, segs=10)
    # Leather handle grip at center
    mb.add_box(-0.06, 0.022, -0.21, 0.06, 0.078, -0.15, UV_DARK_LEATHER)
    # Bowstring
    mb.add_tube([(-0.51, 0.05, 0.0), (0.51, 0.05, 0.0)], [0.005, 0.005], UV_CREAM, segs=6)
    mb.write_glb(out_path, png, roughness=0.68, metallic=0.05)


def build_shot_pouch(out_path: Path, png: bytes):
    """Leather sling-shot pouch with gleaming iron shot pellets."""
    mb = ItemMeshBuilder()
    mb.add_sphere(0.0, 0.11, 0.0, 0.12, 0.11, 0.12, UV_LEATHER, rings=8, segs=14)
    mb.add_frustum_y(0.0, 0.0, 0.18, 0.25, 0.045, 0.075, UV_LEATHER, segs=12)
    mb.add_torus(0.0, 0.19, 0.0, 0.052, 0.011, UV_GOLD, major_segs=14, minor_segs=6)
    for px, pz in ((0.16, 0.06), (0.21, -0.02), (0.14, -0.08), (-0.16, 0.05)):
        mb.add_sphere(px, 0.032, pz, 0.032, 0.032, 0.032, UV_IRON, rings=6, segs=10)
    mb.write_glb(out_path, png, roughness=0.58, metallic=0.32)


def build_boots(out_path: Path, png: bytes):
    """Pair of cuffed medieval leather & iron-shod boots."""
    mb = ItemMeshBuilder()
    for side, sx, yaw_off in ((-1, -0.12, 0.0), (1, 0.12, 0.04)):
        mb.add_box(sx - 0.065, 0.0, -0.14 + yaw_off, sx + 0.065, 0.035, 0.16 + yaw_off, UV_DARK_LEATHER)
        mb.add_box(sx - 0.060, 0.035, -0.12 + yaw_off, sx + 0.060, 0.11, 0.15 + yaw_off, UV_LEATHER)
        mb.add_frustum_y(sx, -0.03 + yaw_off, 0.08, 0.34, 0.064, 0.074, UV_LEATHER, segs=12)
        mb.add_frustum_y(sx, -0.03 + yaw_off, 0.30, 0.36, 0.082, 0.086, UV_DARK_LEATHER, segs=12)
        mb.add_box(sx - 0.068, 0.24, 0.02 + yaw_off, sx + 0.068, 0.27, 0.06 + yaw_off, UV_GOLD)
    mb.write_glb(out_path, png, roughness=0.72, metallic=0.14)


def build_gloves(out_path: Path, png: bytes):
    """Pair of riveted steel & leather adventurer gauntlets."""
    mb = ItemMeshBuilder()
    for sx in (-0.13, 0.13):
        mb.add_box(sx - 0.07, 0.0, -0.15, sx + 0.07, 0.075, 0.03, UV_DARK_LEATHER)
        mb.add_box(sx - 0.075, 0.015, -0.08, sx + 0.075, 0.085, 0.05, UV_STEEL)
        # Fingers
        for f in range(4):
            fx = sx - 0.052 + f * 0.035
            mb.add_box(fx - 0.013, 0.005, 0.03, fx + 0.013, 0.055, 0.15, UV_STEEL)
        # Thumb
        tx = sx + (0.075 if sx < 0 else -0.075)
        mb.add_box(tx - 0.016, 0.008, -0.01, tx + 0.016, 0.052, 0.09, UV_LEATHER)
        # Cuff trim
        mb.add_box(sx - 0.08, 0.0, -0.16, sx + 0.08, 0.085, -0.11, UV_GOLD)
    mb.write_glb(out_path, png, roughness=0.42, metallic=0.62)


def build_helm(out_path: Path, png: bytes):
    """Medieval steel spangenhelm with gold trim & nasal bar."""
    mb = ItemMeshBuilder()
    mb.add_sphere(0, 0.16, 0, 0.17, 0.18, 0.19, UV_STEEL, rings=8, segs=16)
    mb.add_frustum_y(0, 0, 0.0, 0.14, 0.18, 0.175, UV_STEEL, segs=16, cap_top=False)
    mb.add_torus(0, 0.14, 0, 0.178, 0.018, UV_GOLD, major_segs=18, minor_segs=8)
    # Crest ridge & nasal guard
    mb.add_box(-0.020, 0.14, -0.18, 0.020, 0.36, 0.19, UV_GOLD)
    mb.add_box(-0.022, 0.02, 0.16, 0.022, 0.17, 0.21, UV_STEEL)
    mb.write_glb(out_path, png, roughness=0.32, metallic=0.82)


def build_crown(out_path: Path, png: bytes):
    """Royal golden jeweled crown with 6 spires and ruby/emerald/sapphire gems."""
    mb = ItemMeshBuilder()
    mb.add_torus(0, 0.06, 0, 0.16, 0.036, UV_GOLD, major_segs=20, minor_segs=10)
    mb.add_frustum_y(0, 0, 0.04, 0.15, 0.168, 0.176, UV_GOLD, segs=18, cap_bot=False, cap_top=False)
    gems = (UV_RUBY, UV_STARLIGHT, UV_EMERALD)
    for s in range(6):
        a = 2.0 * math.pi * s / 6.0
        px, pz = math.cos(a) * 0.165, math.sin(a) * 0.165
        mb.add_frustum_y(px, pz, 0.14, 0.26, 0.036, 0.004, UV_GOLD, segs=6, cap_top=False)
        mb.add_sphere(px * 1.07, 0.10, pz * 1.07, 0.024, 0.028, 0.024, gems[s % 3], rings=6, segs=8)
    mb.write_glb(out_path, png, roughness=0.22, metallic=0.88)


def build_cloak(out_path: Path, png: bytes):
    """Folded forest-green ranger cloak fastened with a golden brooch."""
    mb = ItemMeshBuilder()
    mb.add_box(-0.24, 0.0, -0.18, 0.24, 0.07, 0.18, UV_CLOAK)
    mb.add_box(-0.21, 0.07, -0.15, 0.21, 0.13, 0.15, UV_CLOAK)
    # Hood fold resting on top
    mb.add_sphere(0.0, 0.15, -0.04, 0.14, 0.065, 0.11, UV_CLOAK, rings=6, segs=12)
    # Golden clasp brooch & trim
    mb.add_torus(0.0, 0.14, 0.09, 0.042, 0.012, UV_GOLD, major_segs=14, minor_segs=6)
    mb.add_sphere(0.0, 0.145, 0.09, 0.022, 0.018, 0.022, UV_EMERALD, rings=6, segs=8)
    mb.write_glb(out_path, png, roughness=0.88, metallic=0.06)


def build_armor(out_path: Path, png: bytes, body_uv, trim_uv, pauldron_uv, roughness=0.45, metallic=0.65):
    """Standing sculpted medieval cuirass / armor tunic (used for Leather, Plate, Dragon Scale)."""
    mb = ItemMeshBuilder()
    # Torso cuirass
    mb.add_frustum_y(0, 0, 0.0, 0.48, 0.17, 0.22, body_uv, segs=12)
    # Chest ridge & belt
    mb.add_box(-0.20, 0.06, -0.18, 0.20, 0.13, 0.18, trim_uv)
    mb.add_box(-0.05, 0.05, 0.16, 0.05, 0.14, 0.21, UV_GOLD)
    # Pauldrons (shoulder armor)
    mb.add_sphere(-0.24, 0.42, 0.0, 0.11, 0.10, 0.13, pauldron_uv, rings=7, segs=12)
    mb.add_sphere(0.24, 0.42, 0.0, 0.11, 0.10, 0.13, pauldron_uv, rings=7, segs=12)
    # Gorget collar
    mb.add_torus(0, 0.49, 0, 0.13, 0.028, trim_uv, major_segs=16, minor_segs=8)
    mb.write_glb(out_path, png, roughness=roughness, metallic=metallic)


def build_amulet(out_path: Path, png: bytes):
    """Golden chain necklace with glowing emerald pendant medallion."""
    mb = ItemMeshBuilder()
    mb.add_torus(0, 0.03, -0.05, 0.15, 0.014, UV_GOLD, major_segs=20, minor_segs=8)
    mb.add_box(-0.065, 0.01, 0.07, 0.065, 0.055, 0.20, UV_GOLD)
    mb.add_sphere(0.0, 0.055, 0.135, 0.042, 0.024, 0.048, UV_EMERALD, rings=6, segs=10)
    mb.write_glb(out_path, png, roughness=0.22, metallic=0.86)


def build_ring(out_path: Path, png: bytes):
    """Gleaming golden ring with raised sapphire bezel stone."""
    mb = ItemMeshBuilder()
    mb.add_torus(0, 0.10, 0, 0.10, 0.022, UV_GOLD, major_segs=20, minor_segs=10, tilt_x=0.95)
    mb.add_box(-0.036, 0.155, 0.035, 0.036, 0.215, 0.105, UV_GOLD)
    mb.add_sphere(0.0, 0.21, 0.075, 0.034, 0.032, 0.034, UV_STARLIGHT, rings=7, segs=10)
    mb.write_glb(out_path, png, roughness=0.18, metallic=0.90)


def build_rod(out_path: Path, png: bytes):
    """Runed silver-mithril arcane rod with glowing sapphire crystal tip."""
    mb = ItemMeshBuilder()
    mb.add_tube([(-0.34, 0.05, 0.0), (0.28, 0.05, 0.0)], [0.022, 0.026], UV_MITHRIL, segs=12)
    for rx in (-0.28, -0.08, 0.12, 0.24):
        mb.add_box(rx - 0.015, 0.018, -0.032, rx + 0.015, 0.082, 0.032, UV_GOLD)
    mb.add_sphere(0.34, 0.05, 0.0, 0.062, 0.046, 0.046, UV_STARLIGHT, rings=7, segs=12)
    mb.write_glb(out_path, png, roughness=0.25, metallic=0.80)


def build_item_mushroom(out_path: Path, png: bytes):
    """Cluster of 3 plump spotted fantasy dungeon mushrooms."""
    mb = ItemMeshBuilder()
    specs = [(0.0, 0.0, 0.24, 0.14), (-0.14, 0.09, 0.16, 0.095), (0.13, -0.08, 0.14, 0.085)]
    for cx, cz, h, cap_r in specs:
        mb.add_frustum_y(cx, cz, 0.0, h * 0.72, cap_r * 0.34, cap_r * 0.24, UV_CREAM, segs=10)
        mb.add_sphere(cx, h * 0.72, cz, cap_r, cap_r * 0.58, cap_r, UV_MUSH_CAP, rings=7, segs=14)
    mb.write_glb(out_path, png, roughness=0.68, metallic=0.04)


def generate_all(raw_root: Path) -> int:
    items_dir = raw_root / "items"
    items_dir.mkdir(parents=True, exist_ok=True)
    atlas = make_png(256, 256, _item_atlas_rgb)

    build_torch_hand(items_dir / "abfp_torch_hand.glb", atlas)
    build_phial(items_dir / "abfp_phial.glb", atlas)
    build_mace(items_dir / "abfp_mace.glb", atlas)
    build_spear(items_dir / "abfp_spear.glb", atlas)
    build_pickaxe(items_dir / "abfp_pickaxe.glb", atlas)
    build_bow(items_dir / "abfp_bow.glb", atlas)
    build_shot_pouch(items_dir / "abfp_shot_pouch.glb", atlas)
    build_boots(items_dir / "abfp_boots.glb", atlas)
    build_gloves(items_dir / "abfp_gloves.glb", atlas)
    build_helm(items_dir / "abfp_helm.glb", atlas)
    build_crown(items_dir / "abfp_crown.glb", atlas)
    build_cloak(items_dir / "abfp_cloak.glb", atlas)
    build_armor(items_dir / "abfp_leather_armor.glb", atlas, UV_LEATHER, UV_DARK_LEATHER, UV_LEATHER, roughness=0.76, metallic=0.10)
    build_armor(items_dir / "abfp_plate_armor.glb", atlas, UV_STEEL, UV_IRON, UV_STEEL, roughness=0.30, metallic=0.85)
    build_armor(items_dir / "abfp_dragon_armor.glb", atlas, UV_DRAGON, UV_GOLD, UV_DRAGON, roughness=0.34, metallic=0.65)
    build_amulet(items_dir / "abfp_amulet.glb", atlas)
    build_ring(items_dir / "abfp_ring.glb", atlas)
    build_rod(items_dir / "abfp_rod.glb", atlas)
    build_item_mushroom(items_dir / "abfp_item_mushroom.glb", atlas)
    return 19


if __name__ == "__main__":
    repo = Path(__file__).resolve().parent.parent
    n = generate_all(repo / "assets" / "raw")
    print(f"Generated {n} bespoke Angband item & viewmodel GLBs.")
