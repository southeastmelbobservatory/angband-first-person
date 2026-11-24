#!/usr/bin/env python3
"""Generate bespoke CC0 procedural 3D GLB meshes with embedded painted PNG textures
for iconic classic Angband dungeon creatures that lack accurate external CC0 models:
  - Floating Eye (hovering veiny eyeball + iris/pupil + trailing optic tendrils)
  - Mushroom Patch (multi-cap spotted toadstool cluster on mossy mound)
  - Fungal Mold (spreading cluster of bumpy mold cushions & spore pods)
  - Giant Ant (segmented head/mandibles/antennae/thorax/gaster + 6 jointed legs)
  - Giant Centipede (S-curved 11-segment armored body + 22 legs + pincers/antennae)
  - Worm Mass (tangled writhing cluster of 8 segmented curving worms)
  - Giant Louse / Beetle (domed carapace shell + mandibles + 6 legs)
  - Dungeon Slime / Jelly (clean organic glistening dome + pseudopods, no floor plane)
  - Giant Frog (crouching warty frog + protruding golden eyes + bent hind legs)
  - Reared Cobra / Serpent (coiled base + tall arched hooded strike pose)
  - Gelatinous Cube (beveled rippling cube with suspended internal core)

Uses Python 3 standard library ONLY (math, struct, json, zlib, random) so it runs
reliably inside Windows Python, WSL, and Unreal Engine's embedded Python interpreter.
"""

import json
import math
import random
import struct
import zlib
from pathlib import Path


def v_add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def v_sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def v_scale(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def v_cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def v_dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def v_norm(a):
    l = math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2])
    return (a[0] / l, a[1] / l, a[2] / l) if l > 1e-8 else (0.0, 1.0, 0.0)


def encode_png_rgb(w: int, h: int, pixels_rgb: bytearray) -> bytes:
    raw = bytearray()
    row_bytes = w * 3
    for y in range(h):
        raw.append(0)
        start = y * row_bytes
        raw.extend(pixels_rgb[start : start + row_bytes])

    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    return (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(bytes(raw), 6))
        + chunk(b"IEND", b"")
    )


def make_palette_atlas_png(swatches: list[tuple[tuple[float, float, float], tuple[float, float, float], str]], size: int = 128) -> bytes:
    """Build an N-band horizontal textured palette PNG (width=size*N, height=size) with rich organic
    mottling, stripes, spots, or iris patterns per band so every part gets real surface texture detail."""
    n = len(swatches)
    w = size * n
    h = size
    buf = bytearray(w * h * 3)

    for bi, (c_base, c_accent, pattern) in enumerate(swatches):
        x0 = bi * size
        for py in range(h):
            v = py / float(h - 1)
            for px in range(size):
                u = px / float(size - 1)
                # Deterministic multi-frequency organic noise
                n1 = math.sin(u * 19.0 + v * 13.0) * math.cos(u * 11.0 - v * 23.0)
                n2 = math.sin(u * 43.0 - v * 37.0) * 0.5
                t = 0.5 + 0.35 * n1 + 0.15 * n2

                if pattern == "eye":
                    # Polar coordinates centered on front of sphere (u=0.5, v=0.5)
                    du = (u - 0.5) * 2.0
                    dv = (v - 0.5) * 2.0
                    r = math.sqrt(du * du + dv * dv)
                    ang = math.atan2(dv, du)
                    if r < 0.18:
                        col = (0.03, 0.02, 0.04)  # Deep black pupil
                    elif r < 0.22:
                        col = (0.95, 0.72, 0.15)  # Bright golden inner ring
                    elif r < 0.44:
                        # Striated glowing crimson-amber iris
                        striation = 0.55 + 0.45 * math.sin(ang * 26.0 + r * 14.0)
                        col = (
                            c_accent[0] * striation + 0.12,
                            c_accent[1] * striation,
                            c_accent[2] * striation,
                        )
                    elif r < 0.48:
                        col = (0.14, 0.03, 0.03)  # Limbal ring
                    else:
                        # Cream sclera with fine bloodshot veins radiating outward
                        vein = max(0.0, math.sin(ang * 14.0 + math.sin(r * 18.0) * 1.4) - 0.72) * 3.2
                        vein *= min(1.0, (r - 0.48) * 2.5)
                        col = (
                            c_base[0] * (1.0 - vein * 0.2) + 0.65 * vein,
                            c_base[1] * (1.0 - vein * 0.75),
                            c_base[2] * (1.0 - vein * 0.78),
                        )
                elif pattern == "spots":
                    # Toadstool spotted cap
                    spot = 0.0
                    for su, sv, sr in (
                        (0.22, 0.28, 0.11), (0.65, 0.22, 0.12), (0.45, 0.55, 0.13),
                        (0.82, 0.62, 0.10), (0.18, 0.74, 0.11), (0.58, 0.84, 0.09),
                    ):
                        d = math.sqrt((u - su) ** 2 + (v - sv) ** 2)
                        if d < sr:
                            spot = max(spot, 1.0 - (d / sr) ** 2)
                    mix = min(1.0, spot * 1.15)
                    col = tuple(c_base[k] * (1.0 - mix) + c_accent[k] * mix for k in range(3))
                    col = tuple(c * (0.88 + 0.22 * t) for c in col)
                elif pattern == "rings":
                    # Segmented worm/chitin stripes
                    band = 0.5 + 0.5 * math.sin(v * math.pi * 16.0 + n1 * 0.6)
                    mix = band * 0.65
                    col = tuple(c_base[k] * (1.0 - mix) + c_accent[k] * mix for k in range(3))
                elif pattern == "scales":
                    # Diamond reptile scales
                    s1 = math.sin((u + v) * math.pi * 14.0)
                    s2 = math.sin((u - v) * math.pi * 14.0)
                    mix = max(0.0, s1 * s2) * 0.75
                    col = tuple(c_base[k] * (1.0 - mix) + c_accent[k] * mix for k in range(3))
                elif pattern == "bubbles":
                    # Translucent-feel slime bubbles
                    bub = 0.0
                    for su, sv, sr in (
                        (0.25, 0.35, 0.09), (0.72, 0.48, 0.12), (0.48, 0.72, 0.08),
                        (0.62, 0.22, 0.07), (0.18, 0.68, 0.07), (0.85, 0.78, 0.06),
                    ):
                        d = math.sqrt((u - su) ** 2 + (v - sv) ** 2)
                        if d < sr:
                            bub = max(bub, 0.4 + 0.6 * (1.0 - d / sr))
                    mix = min(1.0, bub * 0.75 + t * 0.25)
                    col = tuple(c_base[k] * (1.0 - mix) + c_accent[k] * mix for k in range(3))
                else:
                    # Organic mottled shading
                    mix = max(0.0, min(1.0, t))
                    col = tuple(c_base[k] * (1.0 - mix * 0.45) + c_accent[k] * (mix * 0.45) for k in range(3))

                idx = (py * w + (x0 + px)) * 3
                buf[idx] = max(0, min(255, int(col[0] * 255.0)))
                buf[idx + 1] = max(0, min(255, int(col[1] * 255.0)))
                buf[idx + 2] = max(0, min(255, int(col[2] * 255.0)))

    return encode_png_rgb(w, h, buf)


class MeshBuilder:
    def __init__(self, num_bands: int):
        self.num_bands = max(1, num_bands)
        self.pos = []
        self.nor = []
        self.uv = []
        self.idx = []

    def _band_uv(self, band: int, u: float, v: float):
        # Map u in [0.02..0.98] inside the swatch band to avoid cross-band bleeding
        u_c = 0.02 + 0.96 * max(0.0, min(1.0, u))
        return ((band + u_c) / self.num_bands, max(0.02, min(0.98, v)))

    def add_ellipsoid(self, center, radii, band: int, seg_u: int = 16, seg_v: int = 12,
                      rot_xyz=(0.0, 0.0, 0.0), deform_fn=None, eye_front_uv: bool = False):
        cx, cy, cz = center
        rx, ry, rz = radii
        ax, ay, az = rot_xyz
        cxr, sxr = math.cos(ax), math.sin(ax)
        cyr, syr = math.cos(ay), math.sin(ay)
        czr, szr = math.cos(az), math.sin(az)

        def rot_p(x, y, z):
            y, z = y * cxr - z * sxr, y * sxr + z * cxr
            x, z = x * cyr + z * syr, -x * syr + z * cyr
            x, y = x * czr - y * szr, x * szr + y * czr
            return (x, y, z)

        base = len(self.pos)
        for iv in range(seg_v + 1):
            fv = iv / float(seg_v)
            phi = fv * math.pi  # 0 (top) .. pi (bottom)
            sp, cp = math.sin(phi), math.cos(phi)
            for iu in range(seg_u + 1):
                fu = iu / float(seg_u)
                theta = fu * 2.0 * math.pi
                st, ct = math.sin(theta), math.cos(theta)
                nx, ny, nz = sp * st, cp, sp * ct
                x, y, z = nx * rx, ny * ry, nz * rz
                if deform_fn:
                    x, y, z = deform_fn(x, y, z, fu, fv)
                rxp, ryp, rzp = rot_p(x, y, z)
                rnx, rny, rnz = v_norm(rot_p(nx / max(rx, 1e-4), ny / max(ry, 1e-4), nz / max(rz, 1e-4)))
                self.pos.append((cx + rxp, cy + ryp, cz + rzp))
                self.nor.append((rnx, rny, rnz))
                if eye_front_uv:
                    # Front of eyeball (+Z) maps right onto center of eye swatch (0.5, 0.5)
                    if nz >= 0.0:
                        eu = 0.5 + nx * 0.48
                        ev = 0.5 - ny * 0.48
                    else:
                        eu = 0.04
                        ev = 0.04
                    self.uv.append(self._band_uv(band, eu, ev))
                else:
                    self.uv.append(self._band_uv(band, fu, fv))

        stride = seg_u + 1
        for iv in range(seg_v):
            for iu in range(seg_u):
                a = base + iv * stride + iu
                b = a + 1
                c = a + stride
                d = c + 1
                self.idx.extend([a, c, b, b, c, d])

    def add_tube(self, points, radii, band: int, radial_seg: int = 10, cap_ends: bool = True):
        """Sweep a smooth circular/organic tube along a 3D polyline."""
        n_pts = len(points)
        if n_pts < 2:
            return
        base = len(self.pos)
        prev_up = (0.0, 1.0, 0.0)
        rings = []

        for i in range(n_pts):
            p = points[i]
            if i == 0:
                tang = v_norm(v_sub(points[1], points[0]))
            elif i == n_pts - 1:
                tang = v_norm(v_sub(points[-1], points[-2]))
            else:
                tang = v_norm(v_sub(points[i + 1], points[i - 1]))

            if abs(v_dot(tang, prev_up)) > 0.92:
                prev_up = (1.0, 0.0, 0.0) if abs(tang[0]) < 0.8 else (0.0, 0.0, 1.0)
            right = v_norm(v_cross(tang, prev_up))
            up = v_norm(v_cross(right, tang))
            prev_up = up

            r = radii[i] if isinstance(radii, (list, tuple)) else radii
            rx, ry = (r[0], r[1]) if isinstance(r, (list, tuple)) else (r, r)
            fv = i / float(n_pts - 1)
            ring_start = len(self.pos)
            for j in range(radial_seg + 1):
                fu = j / float(radial_seg)
                ang = fu * 2.0 * math.pi
                ca, sa = math.cos(ang), math.sin(ang)
                n_dir = v_norm(v_add(v_scale(right, ca), v_scale(up, sa)))
                offset = v_add(v_scale(right, ca * rx), v_scale(up, sa * ry))
                self.pos.append(v_add(p, offset))
                self.nor.append(n_dir)
                self.uv.append(self._band_uv(band, fu, fv))
            rings.append(ring_start)

        stride = radial_seg + 1
        for i in range(n_pts - 1):
            r0, r1 = rings[i], rings[i + 1]
            for j in range(radial_seg):
                a, b = r0 + j, r0 + j + 1
                c, d = r1 + j, r1 + j + 1
                self.idx.extend([a, c, b, b, c, d])

        if cap_ends:
            # Start cap
            p0 = points[0]
            t0 = v_norm(v_sub(points[0], points[1]))
            c0 = len(self.pos)
            self.pos.append(p0)
            self.nor.append(t0)
            self.uv.append(self._band_uv(band, 0.5, 0.0))
            for j in range(radial_seg):
                self.idx.extend([c0, rings[0] + j, rings[0] + j + 1])
            # End cap
            pe = points[-1]
            te = v_norm(v_sub(points[-1], points[-2]))
            ce = len(self.pos)
            self.pos.append(pe)
            self.nor.append(te)
            self.uv.append(self._band_uv(band, 0.5, 1.0))
            for j in range(radial_seg):
                self.idx.extend([ce, rings[-1] + j + 1, rings[-1] + j])

    def add_mushroom(self, base_pos, height: float, cap_r: float, tilt_x: float, tilt_z: float,
                     stem_band: int, cap_band: int, gill_band: int):
        bx, by, bz = base_pos
        # Curved organic stem
        stem_pts = []
        stem_r = []
        for i in range(7):
            t = i / 6.0
            sx = bx + math.sin(t * 1.4) * tilt_x * height * 0.45
            sy = by + t * height
            sz = bz + math.sin(t * 1.4) * tilt_z * height * 0.45
            stem_pts.append((sx, sy, sz))
            r = cap_r * (0.28 * (1.0 - t * 0.35) + (0.06 if i == 0 else 0.0))
            stem_r.append(r)
        self.add_tube(stem_pts, stem_r, stem_band, radial_seg=10)

        top = stem_pts[-1]
        # Domed toadstool cap (upper hemisphere + gentle flare skirt)
        def cap_deform(x, y, z, fu, fv):
            if y < 0.0:
                y *= 0.22
            # Subtle organic rim waviness
            wave = 1.0 + 0.06 * math.sin(fu * math.pi * 10.0) * math.sin(fv * math.pi)
            return (x * wave, y, z * wave)

        self.add_ellipsoid(
            (top[0], top[1] + cap_r * 0.12, top[2]),
            (cap_r, cap_r * 0.62, cap_r),
            cap_band,
            seg_u=18,
            seg_v=12,
            rot_xyz=(tilt_z * 0.35, 0.0, -tilt_x * 0.35),
            deform_fn=cap_deform,
        )
        # Underside gills ring
        self.add_ellipsoid(
            (top[0], top[1] - cap_r * 0.04, top[2]),
            (cap_r * 0.86, cap_r * 0.14, cap_r * 0.86),
            gill_band,
            seg_u=14,
            seg_v=6,
            rot_xyz=(tilt_z * 0.35, 0.0, -tilt_x * 0.35),
        )

    def add_jointed_leg(self, root, mid, foot, r_base: float, band: int):
        pts = [
            root,
            v_add(v_scale(root, 0.5), v_scale(mid, 0.5)),
            mid,
            v_add(v_scale(mid, 0.5), v_scale(foot, 0.5)),
            foot,
        ]
        radii = [r_base, r_base * 0.85, r_base * 0.72, r_base * 0.55, r_base * 0.32]
        self.add_tube(pts, radii, band, radial_seg=6)

    def export_glb(self, out_path: Path, png_bytes: bytes, roughness: float = 0.72, metallic: float = 0.0, ground: bool = True):
        if ground and self.pos:
            xs = [p[0] for p in self.pos]
            ys = [p[1] for p in self.pos]
            zs = [p[2] for p in self.pos]
            cx = 0.5 * (min(xs) + max(xs))
            cz = 0.5 * (min(zs) + max(zs))
            min_y = min(ys)
            self.pos = [(p[0] - cx, p[1] - min_y, p[2] - cz) for p in self.pos]

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
            "asset": {"version": "2.0", "generator": "ABFP Procedural Bestiary"},
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


def gen_floating_eye(out_path: Path):
    tex = make_palette_atlas_png([
        ((0.92, 0.88, 0.80), (0.85, 0.14, 0.08), "eye"),     # 0: Veiny eyeball + crimson iris
        ((0.48, 0.12, 0.16), (0.28, 0.06, 0.10), "mottle"),  # 1: Optic nerve tendrils
    ])
    mb = MeshBuilder(2)
    mb.add_ellipsoid((0.0, 0.65, 0.0), (0.48, 0.48, 0.48), band=0, seg_u=28, seg_v=20, eye_front_uv=True)
    # 7 trailing sinuous optic nerve tendrils below/behind the floating eye
    for k in range(7):
        ang = (k / 7.0) * 2.0 * math.pi + 0.3
        pts = []
        rads = []
        for s in range(9):
            t = s / 8.0
            r_out = 0.12 + 0.22 * math.sin(t * 2.2) + (0.14 if k > 0 else 0.0)
            px = math.cos(ang + t * 0.9) * r_out
            py = 0.28 - t * 0.72
            pz = -0.14 - t * 0.30 + math.sin(ang + t * 1.4) * 0.10
            pts.append((px, py, pz))
            rads.append(0.055 * (1.0 - t * 0.78))
        mb.add_tube(pts, rads, band=1, radial_seg=7)
    mb.export_glb(out_path, tex, roughness=0.22)


def gen_mushroom_patch(out_path: Path):
    tex = make_palette_atlas_png([
        ((0.58, 0.16, 0.12), (0.94, 0.88, 0.72), "spots"),   # 0: Spotted toadstool cap
        ((0.80, 0.74, 0.58), (0.62, 0.54, 0.40), "mottle"),  # 1: Fibrous cream stem
        ((0.46, 0.36, 0.24), (0.30, 0.22, 0.15), "rings"),   # 2: Underside gills
        ((0.20, 0.26, 0.13), (0.13, 0.16, 0.08), "mottle"),  # 3: Mossy humus base
        ((0.72, 0.52, 0.16), (0.95, 0.86, 0.54), "spots"),   # 4: Ochre-spotted variant cap
    ])
    mb = MeshBuilder(5)
    # Damp mossy mound base
    mb.add_ellipsoid((0.0, 0.08, 0.0), (0.82, 0.14, 0.78), band=3, seg_u=16, seg_v=8)
    # 5 varied toadstools
    specs = [
        ((0.02, 0.10, 0.02),  0.72, 0.44,  0.10,  0.08, 0),
        ((-0.40, 0.08, 0.22), 0.48, 0.30, -0.32,  0.22, 4),
        ((0.38, 0.08, 0.18),  0.54, 0.32,  0.28,  0.18, 0),
        ((-0.24, 0.08, -0.34),0.44, 0.26, -0.22, -0.28, 4),
        ((0.28, 0.08, -0.30), 0.36, 0.22,  0.25, -0.26, 0),
    ]
    for pos, h, r, tx, tz, cband in specs:
        mb.add_mushroom(pos, h, r, tx, tz, stem_band=1, cap_band=cband, gill_band=2)
    mb.export_glb(out_path, tex, roughness=0.76)


def gen_fungal_mold(out_path: Path):
    tex = make_palette_atlas_png([
        ((0.36, 0.42, 0.18), (0.58, 0.62, 0.22), "mottle"),  # 0: Fuzzy olive-ochre mold cushions
        ((0.22, 0.24, 0.16), (0.14, 0.16, 0.10), "mottle"),  # 1: Dark mycelial mat
        ((0.78, 0.68, 0.22), (0.95, 0.85, 0.38), "bubbles"), # 2: Glowing spore pustules
    ])
    mb = MeshBuilder(3)
    rng = random.Random(42)
    # Creeping fungal mat
    mb.add_ellipsoid((0.0, 0.06, 0.0), (0.95, 0.10, 0.92), band=1, seg_u=18, seg_v=8)
    # 14 overlapping lumpy mold cushions
    for k in range(14):
        ang = k * 2.39996
        dist = 0.08 + 0.56 * math.sqrt((k + 0.5) / 14.0)
        rx = 0.20 + rng.uniform(0.0, 0.16)
        ry = 0.18 + (0.28 if k < 4 else rng.uniform(0.0, 0.18))
        rz = 0.20 + rng.uniform(0.0, 0.16)
        px = math.cos(ang) * dist
        pz = math.sin(ang) * dist
        mb.add_ellipsoid((px, ry * 0.65, pz), (rx, ry, rz), band=0, seg_u=12, seg_v=9)
        if k % 2 == 0:
            # Spore pod protruding from cushion
            mb.add_ellipsoid((px, ry * 1.25, pz), (rx * 0.42, ry * 0.42, rz * 0.42), band=2, seg_u=10, seg_v=8)
    mb.export_glb(out_path, tex, roughness=0.88)


def gen_giant_ant(out_path: Path):
    tex = make_palette_atlas_png([
        ((0.24, 0.08, 0.04), (0.42, 0.16, 0.06), "rings"),   # 0: Armored chitin carapace
        ((0.14, 0.05, 0.03), (0.26, 0.10, 0.05), "mottle"),  # 1: Legs & mandibles
        ((0.88, 0.24, 0.08), (0.25, 0.04, 0.02), "mottle"),  # 2: Compound eyes
    ])
    mb = MeshBuilder(3)
    # Abdomen (gaster) in back (-Z), Thorax center, Head forward (+Z)
    mb.add_ellipsoid((0.0, 0.46, -0.68), (0.34, 0.30, 0.48), band=0, seg_u=18, seg_v=14, rot_xyz=(-0.18, 0.0, 0.0))
    mb.add_ellipsoid((0.0, 0.40, -0.20), (0.14, 0.13, 0.18), band=0, seg_u=12, seg_v=8)  # Petiole
    mb.add_ellipsoid((0.0, 0.48,  0.12), (0.22, 0.22, 0.30), band=0, seg_u=16, seg_v=12)  # Thorax
    mb.add_ellipsoid((0.0, 0.56,  0.54), (0.24, 0.21, 0.24), band=0, seg_u=16, seg_v=12, rot_xyz=(0.22, 0.0, 0.0))  # Head
    # Compound eyes
    for side in (-1.0, 1.0):
        mb.add_ellipsoid((side * 0.19, 0.62, 0.62), (0.065, 0.065, 0.075), band=2, seg_u=10, seg_v=8)
        # Curved serrated mandibles
        mb.add_tube(
            [(side * 0.10, 0.48, 0.72), (side * 0.17, 0.44, 0.88), (side * 0.06, 0.42, 0.98)],
            [0.048, 0.036, 0.015], band=1, radial_seg=6,
        )
        # Bent antennae
        mb.add_tube(
            [(side * 0.09, 0.64, 0.72), (side * 0.26, 0.84, 0.94), (side * 0.40, 0.74, 1.18)],
            [0.024, 0.018, 0.010], band=1, radial_seg=6,
        )
        # 6 jointed insect legs planted solidly at Y = 0
        for z_root, z_foot, x_mid, x_foot in (
            ( 0.26,  0.56, 0.52, 0.78),
            ( 0.10,  0.08, 0.62, 0.92),
            (-0.08, -0.58, 0.58, 0.86),
        ):
            mb.add_jointed_leg(
                (side * 0.16, 0.40, z_root),
                (side * x_mid, 0.52, (z_root + z_foot) * 0.5),
                (side * x_foot, 0.0, z_foot),
                0.046, band=1,
            )
    mb.export_glb(out_path, tex, roughness=0.34, metallic=0.12)


def gen_giant_centipede(out_path: Path):
    tex = make_palette_atlas_png([
        ((0.46, 0.34, 0.14), (0.18, 0.12, 0.06), "rings"),   # 0: Banded armored carapace segments
        ((0.28, 0.16, 0.07), (0.14, 0.08, 0.04), "mottle"),  # 1: Legs & mandibles
        ((0.90, 0.20, 0.08), (0.30, 0.05, 0.02), "mottle"),  # 2: Eyes
    ])
    mb = MeshBuilder(3)
    n_seg = 11
    seg_centers = []
    for i in range(n_seg):
        t = i / float(n_seg - 1)
        z = 1.15 - t * 2.30
        x = math.sin(t * math.pi * 1.65) * 0.42
        y = 0.22 + (0.08 if i == 0 else 0.0)
        seg_centers.append((x, y, z))
        width = 0.22 * (0.78 + 0.22 * math.sin(t * math.pi))
        mb.add_ellipsoid((x, y, z), (width, 0.15, 0.19), band=0, seg_u=14, seg_v=10)
        # Pair of splayed legs on every body segment
        if i > 0:
            dx = seg_centers[i - 1][0] - x
            dz = seg_centers[i - 1][2] - z
            tang = v_norm((dx, 0.0, dz))
            side_v = (-tang[2], 0.0, tang[0])
            for side in (-1.0, 1.0):
                root = (x + side_v[0] * side * width * 0.8, y - 0.02, z + side_v[2] * side * width * 0.8)
                mid = (x + side_v[0] * side * (width + 0.22), y + 0.08, z + side_v[2] * side * (width + 0.22))
                foot = (x + side_v[0] * side * (width + 0.38), 0.0, z + side_v[2] * side * (width + 0.38))
                mb.add_jointed_leg(root, mid, foot, 0.032, band=1)

    # Head pincers, eyes, and sweeping antennae on segment 0
    hx, hy, hz = seg_centers[0]
    for side in (-1.0, 1.0):
        mb.add_ellipsoid((hx + side * 0.14, hy + 0.08, hz + 0.12), (0.045, 0.045, 0.045), band=2, seg_u=8, seg_v=6)
        mb.add_tube(
            [(hx + side * 0.09, hy - 0.02, hz + 0.15), (hx + side * 0.18, hy - 0.03, hz + 0.32), (hx + side * 0.07, hy - 0.03, hz + 0.42)],
            [0.038, 0.028, 0.012], band=1, radial_seg=6,
        )
        mb.add_tube(
            [(hx + side * 0.08, hy + 0.06, hz + 0.16), (hx + side * 0.28, hy + 0.22, hz + 0.46), (hx + side * 0.46, hy + 0.18, hz + 0.72)],
            [0.020, 0.014, 0.008], band=1, radial_seg=6,
        )
    mb.export_glb(out_path, tex, roughness=0.32, metallic=0.22)


def gen_worm_mass(out_path: Path):
    tex = make_palette_atlas_png([
        ((0.76, 0.62, 0.54), (0.52, 0.36, 0.32), "rings"),   # 0: Pale ringed worm skin
        ((0.64, 0.56, 0.42), (0.40, 0.34, 0.24), "rings"),   # 1: Earth-ochre worm variant
        ((0.28, 0.22, 0.18), (0.16, 0.12, 0.10), "mottle"),  # 2: Damp organic heap
    ])
    mb = MeshBuilder(3)
    mb.add_ellipsoid((0.0, 0.16, 0.0), (0.68, 0.20, 0.66), band=2, seg_u=16, seg_v=8)
    # 9 tangled writhing segmented worms rising out of the mass
    for k in range(9):
        ang = (k / 9.0) * 2.0 * math.pi
        pts = []
        rads = []
        for s in range(11):
            t = s / 10.0
            r = 0.56 * (1.0 - t * 0.55) + 0.14 * math.sin(t * 5.2 + k)
            a = ang + t * 2.4 + math.sin(t * 4.0 + k * 1.3) * 0.35
            px = math.cos(a) * r
            pz = math.sin(a) * r
            py = 0.08 + math.sin(t * math.pi) * (0.48 + (k % 3) * 0.14) + t * 0.22
            pts.append((px, py, pz))
            # Ringed bulge profile
            seg_bulge = 1.0 + 0.14 * math.sin(t * math.pi * 10.0)
            rads.append((0.075 - t * 0.032) * seg_bulge)
        mb.add_tube(pts, rads, band=(k % 2), radial_seg=8)
    mb.export_glb(out_path, tex, roughness=0.42)


def gen_louse_beetle(out_path: Path):
    tex = make_palette_atlas_png([
        ((0.62, 0.58, 0.48), (0.36, 0.32, 0.26), "rings"),   # 0: Pale-mottled armored shell
        ((0.24, 0.20, 0.16), (0.12, 0.10, 0.08), "mottle"),  # 1: Legs & head
    ])
    mb = MeshBuilder(2)
    # Wide domed ribbed carapace
    def shell_deform(x, y, z, fu, fv):
        if y < 0.0:
            y *= 0.25
        return (x, y, z)
    mb.add_ellipsoid((0.0, 0.24, -0.08), (0.42, 0.30, 0.54), band=0, seg_u=18, seg_v=12, deform_fn=shell_deform)
    mb.add_ellipsoid((0.0, 0.22,  0.46), (0.20, 0.16, 0.20), band=1, seg_u=12, seg_v=8)
    for side in (-1.0, 1.0):
        mb.add_tube(
            [(side * 0.08, 0.22, 0.60), (side * 0.18, 0.24, 0.78)],
            [0.022, 0.010], band=1, radial_seg=6,
        )
        for zr, zf, xm, xf in ((0.26, 0.42, 0.48, 0.64), (0.0, 0.02, 0.56, 0.72), (-0.28, -0.44, 0.50, 0.66)):
            mb.add_jointed_leg((side * 0.26, 0.20, zr), (side * xm, 0.26, (zr + zf) * 0.5), (side * xf, 0.0, zf), 0.038, band=1)
    mb.export_glb(out_path, tex, roughness=0.45)


def gen_dungeon_slime(out_path: Path):
    tex = make_palette_atlas_png([
        ((0.12, 0.58, 0.32), (0.46, 0.92, 0.62), "bubbles"),  # 0: Glistening emerald-teal slime
        ((0.06, 0.28, 0.16), (0.18, 0.48, 0.28), "mottle"),   # 1: Dense internal nucleus
    ])
    mb = MeshBuilder(2)
    def slime_deform(x, y, z, fu, fv):
        # Flatten base smoothly onto floor with undulating pseudopod skirt (NO flat square plane!)
        if y < -0.35:
            y = -0.35 + (y + 0.35) * 0.12
        skirt = max(0.0, (-y) / 0.55)
        lobe = 1.0 + 0.18 * skirt * math.sin(fu * math.pi * 10.0) + 0.08 * math.cos(fu * math.pi * 6.0 + fv * 5.0)
        return (x * lobe, y, z * lobe)

    mb.add_ellipsoid((0.0, 0.42, 0.0), (0.58, 0.52, 0.58), band=0, seg_u=24, seg_v=18, deform_fn=slime_deform)
    # 5 organic oozing side droplets/pseudopods
    for k in range(5):
        ang = k * 1.256 + 0.2
        mb.add_ellipsoid(
            (math.cos(ang) * 0.48, 0.12, math.sin(ang) * 0.48),
            (0.22, 0.14, 0.22), band=0, seg_u=12, seg_v=8,
        )
    mb.export_glb(out_path, tex, roughness=0.14)


def gen_giant_frog(out_path: Path):
    tex = make_palette_atlas_png([
        ((0.18, 0.46, 0.16), (0.34, 0.56, 0.22), "spots"),   # 0: Mottled warty green frog skin
        ((0.68, 0.72, 0.46), (0.48, 0.54, 0.32), "mottle"),  # 1: Pale belly
        ((0.92, 0.74, 0.10), (0.08, 0.06, 0.04), "eye"),     # 2: Protruding golden frog eyes
    ])
    mb = MeshBuilder(3)
    # Angled plump crouching torso
    mb.add_ellipsoid((0.0, 0.36, 0.0), (0.44, 0.30, 0.52), band=0, seg_u=18, seg_v=14, rot_xyz=(-0.38, 0.0, 0.0))
    mb.add_ellipsoid((0.0, 0.26, 0.10), (0.36, 0.20, 0.42), band=1, seg_u=14, seg_v=10, rot_xyz=(-0.38, 0.0, 0.0))
    # Wide flat frog head + jaw
    mb.add_ellipsoid((0.0, 0.50, 0.46), (0.36, 0.20, 0.30), band=0, seg_u=16, seg_v=12)
    for side in (-1.0, 1.0):
        # Protruding eyes on top of head
        mb.add_ellipsoid((side * 0.20, 0.68, 0.50), (0.10, 0.10, 0.10), band=2, seg_u=14, seg_v=10, eye_front_uv=True)
        # Muscular folded hind legs ready to spring
        mb.add_ellipsoid((side * 0.42, 0.24, -0.26), (0.16, 0.18, 0.30), band=0, seg_u=12, seg_v=8, rot_xyz=(0.35, side * 0.25, 0.0))
        mb.add_tube(
            [(side * 0.46, 0.16, -0.34), (side * 0.52, 0.06, -0.02), (side * 0.54, 0.04, 0.18)],
            [0.09, 0.07, 0.055], band=0, radial_seg=7,
        )
        # Front bracing arms
        mb.add_jointed_leg((side * 0.30, 0.36, 0.32), (side * 0.38, 0.22, 0.44), (side * 0.32, 0.03, 0.54), 0.075, band=0)
    mb.export_glb(out_path, tex, roughness=0.36)


def gen_reared_cobra(out_path: Path):
    tex = make_palette_atlas_png([
        ((0.20, 0.42, 0.18), (0.48, 0.62, 0.24), "scales"),  # 0: Scaled green-gold dorsal skin
        ((0.76, 0.68, 0.42), (0.58, 0.48, 0.28), "rings"),   # 1: Ribbed cobra hood belly
        ((0.95, 0.72, 0.10), (0.10, 0.04, 0.02), "mottle"),  # 2: Slit golden eyes
    ])
    mb = MeshBuilder(3)
    # Coiled base on floor -> rising S-arched neck -> striking head facing +Z
    pts = []
    rads = []
    # Lower 1.5 coils on floor
    for i in range(18):
        t = i / 17.0
        ang = t * math.pi * 2.6
        rad_coil = 0.58 - t * 0.28
        px = math.cos(ang) * rad_coil
        pz = math.sin(ang) * rad_coil
        py = 0.09 + t * 0.14
        pts.append((px, py, pz))
        rads.append(0.05 + t * 0.07)
    # Rising neck + flared cobra hood
    neck_pts = [
        (0.18, 0.28, -0.10),
        (0.08, 0.52, -0.18),
        (0.00, 0.78, -0.12),
        (0.00, 1.02,  0.04),
        (0.00, 1.18,  0.22),
        (0.00, 1.14,  0.42),
    ]
    neck_rads = [
        (0.11, 0.11),
        (0.12, 0.10),
        (0.24, 0.08),  # Wide flared cobra hood!
        (0.26, 0.08),  # Wide flared cobra hood!
        (0.13, 0.10),  # Head
        (0.06, 0.05),  # Snout
    ]
    pts.extend(neck_pts)
    rads.extend(neck_rads)
    mb.add_tube(pts, rads, band=0, radial_seg=10)
    # Hood chest plate + eyes
    mb.add_ellipsoid((0.0, 0.90, 0.04), (0.24, 0.22, 0.07), band=1, seg_u=14, seg_v=10, rot_xyz=(-0.25, 0.0, 0.0))
    for side in (-1.0, 1.0):
        mb.add_ellipsoid((side * 0.09, 1.21, 0.30), (0.032, 0.032, 0.032), band=2, seg_u=8, seg_v=6)
    mb.export_glb(out_path, tex, roughness=0.35)


def gen_gelatinous_cube(out_path: Path):
    tex = make_palette_atlas_png([
        ((0.28, 0.76, 0.68), (0.62, 0.95, 0.90), "bubbles"),  # 0: Translucent aqua-mint jelly cube
        ((0.86, 0.82, 0.72), (0.64, 0.58, 0.48), "mottle"),   # 1: Trapped bone/skull inside
    ])
    mb = MeshBuilder(2)
    # Rounded super-ellipsoid cube with gently rippling walls
    def cube_deform(x, y, z, fu, fv):
        # Push sphere radially outward toward cube corners (Lp norm p=6)
        m = (abs(x) ** 6 + abs(y) ** 6 + abs(z) ** 6) ** (1.0 / 6.0)
        if m > 1e-5:
            s = 0.65 / m
            x, y, z = x * s, y * s, z * s
        ripple = 1.0 + 0.03 * math.sin(fu * math.pi * 8.0 + fv * math.pi * 6.0)
        return (x * ripple, y * ripple, z * ripple)

    mb.add_ellipsoid((0.0, 0.65, 0.0), (0.65, 0.65, 0.65), band=0, seg_u=24, seg_v=18, deform_fn=cube_deform)
    mb.export_glb(out_path, tex, roughness=0.10)


GENERATORS = {
    "creatures/abfp_floating_eye.glb": gen_floating_eye,
    "creatures/abfp_mushroom_patch.glb": gen_mushroom_patch,
    "creatures/abfp_mold.glb": gen_fungal_mold,
    "creatures/abfp_ant.glb": gen_giant_ant,
    "creatures/abfp_centipede.glb": gen_giant_centipede,
    "creatures/abfp_worm_mass.glb": gen_worm_mass,
    "creatures/abfp_louse_beetle.glb": gen_louse_beetle,
    "creatures/abfp_slime_jelly.glb": gen_dungeon_slime,
    "creatures/abfp_frog.glb": gen_giant_frog,
    "creatures/abfp_cobra.glb": gen_reared_cobra,
    "creatures/abfp_gelatinous_cube.glb": gen_gelatinous_cube,
}


def generate_all(raw_root: Path) -> int:
    count = 0
    for rel, fn in GENERATORS.items():
        dst = raw_root / rel
        fn(dst)
        count += 1
    return count


if __name__ == "__main__":
    repo_root = Path(__file__).resolve().parent.parent
    n = generate_all(repo_root / "assets" / "raw")
    print(f"Generated {n} bespoke Angband dungeon creature GLBs in {repo_root / 'assets' / 'raw' / 'creatures'}")
