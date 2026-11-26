#!/usr/bin/env python3
"""Generate bespoke CC0 procedural 3D GLB meshes with embedded 3-map PBR textures
(Albedo + OpenGL Normal + ORM Occlusion/Roughness/Metallic) for iconic classic
Angband dungeon creatures that lack high-detail external CC0 models:
  - Floating Eye (hovering veiny eyeball + glossy cornea/iris + 9 optic tendrils)
  - Mushroom Patch (multi-cap spotted toadstool cluster + radial gills on mossy mound)
  - Fungal Mold (spreading cluster of bumpy mold cushions & glowing spore pods)
  - Giant Ant (segmented armored carapace, compound eyes, mandibles, antennae, 6 legs)
  - Giant Centipede (S-curved 12-segment armored body + 22 jointed legs + pincers)
  - Worm Mass (tangled writhing cluster of 10 ringed segmented worms)
  - Giant Louse / Beetle (ribbed domed carapace shell + mandibles + 6 legs)
  - Dungeon Slime / Jelly (glistening organic dome + oozing pseudopods)
  - Giant Frog (crouching warty frog + protruding golden eyes + muscular hind legs)
  - Reared Cobra / Serpent (coiled base + flared hooded strike pose + fangs + forked tongue)
  - Gelatinous Cube (rippling glassy cube with suspended internal skull & bones)
  - Cave Spider (armored cephalothorax + patterned abdomen + 8 articulated legs + fangs + 8 crimson eyes)

Also generates shared seamless 256x256 organic micro-detail PBR maps
(_shared_organic_detail.png and _shared_organic_normal.png) used by Unreal's
shared M_ABFP_CreaturePBRMaster material for untextured CC0 beast meshes.

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


def clamp01(x: float) -> float:
    return 0.0 if x < 0.0 else (1.0 if x > 1.0 else x)


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


def _height_to_normal_rgb(heights: list[float], w: int, h: int, strength: float, wrap_x: bool = False,
                          wrap_y: bool = False, flip_y_dx: bool = False, band_width: int = 0) -> bytearray:
    """Convert a scalar heightfield into a tangent-space RGB normal map.
    glTF 2.0 expects OpenGL normals (flip_y_dx=False); UE DirectX TC_NORMALMAP expects flip_y_dx=True."""
    out = bytearray(w * h * 3)
    y_sign = -1.0 if flip_y_dx else 1.0
    for y in range(h):
        ym = (y - 1) % h if wrap_y else max(0, y - 1)
        yp = (y + 1) % h if wrap_y else min(h - 1, y + 1)
        row_m = ym * w
        row_c = y * w
        row_p = yp * w
        for x in range(w):
            if wrap_x:
                xm = (x - 1) % w
                xp = (x + 1) % w
            elif band_width > 0:
                b0 = (x // band_width) * band_width
                b1 = b0 + band_width - 1
                xm = max(b0, x - 1)
                xp = min(b1, x + 1)
            else:
                xm = max(0, x - 1)
                xp = min(w - 1, x + 1)

            dx = (heights[row_c + xp] - heights[row_c + xm]) * strength
            dy = (heights[row_p + x] - heights[row_m + x]) * strength
            nx = -dx
            ny = -dy * y_sign
            nz = 1.0
            inv = 1.0 / math.sqrt(nx * nx + ny * ny + nz * nz)
            idx = (row_c + x) * 3
            out[idx] = max(0, min(255, int((nx * inv * 0.5 + 0.5) * 255.0)))
            out[idx + 1] = max(0, min(255, int((ny * inv * 0.5 + 0.5) * 255.0)))
            out[idx + 2] = max(0, min(255, int((nz * inv * 0.5 + 0.5) * 255.0)))
    return out


def make_pbr_atlas_pngs(swatches: list, size: int = 192) -> tuple[bytes, bytes, bytes]:
    """Build matched N-band horizontal PBR textures (Albedo, OpenGL Normal, ORM) at width=size*N, height=size.
    Each swatch is (c_base, c_accent, pattern[, rough_base[, metallic_base[, bump_strength]]]).
    ORM packs glTF-standard channels: R = Ambient Occlusion, G = Roughness, B = Metallic."""
    n = len(swatches)
    w = size * n
    h = size
    albedo = bytearray(w * h * 3)
    orm = bytearray(w * h * 3)
    heights = [0.5] * (w * h)

    sin = math.sin
    cos = math.cos
    sqrt = math.sqrt
    atan2 = math.atan2
    pi = math.pi
    inv_sz = 1.0 / float(size - 1)

    for bi, sw in enumerate(swatches):
        c_base = sw[0]
        c_accent = sw[1]
        pattern = sw[2]
        rough_base = sw[3] if len(sw) > 3 else 0.58
        metal_base = sw[4] if len(sw) > 4 else 0.0
        bump_str = sw[5] if len(sw) > 5 else 2.4

        x0 = bi * size
        for py in range(h):
            v = py * inv_sz
            row_off = py * w + x0
            for px in range(size):
                u = px * inv_sz
                # Rich 3-octave deterministic organic noise
                n1 = sin(u * 19.0 + v * 13.0) * cos(u * 11.0 - v * 23.0)
                n2 = sin(u * 43.0 - v * 37.0) * cos(u * 29.0 + v * 41.0)
                n3 = sin(u * 89.0 + v * 73.0) * 0.5
                t = 0.5 + 0.28 * n1 + 0.14 * n2 + 0.08 * n3

                hv = t
                rough = rough_base + (0.5 - t) * 0.16
                metal = metal_base
                ao = 0.84 + 0.16 * clamp01(t)

                if pattern == "eye":
                    du = (u - 0.5) * 2.0
                    dv = (v - 0.5) * 2.0
                    r = sqrt(du * du + dv * dv)
                    ang = atan2(dv, du)
                    if r < 0.17:
                        col = (0.02, 0.015, 0.03)
                        hv = 0.72
                        rough = 0.04
                        ao = 1.0
                    elif r < 0.22:
                        col = (0.98, 0.76, 0.18)
                        hv = 0.76
                        rough = 0.06
                    elif r < 0.44:
                        striation = 0.52 + 0.48 * sin(ang * 32.0 + r * 18.0 + n1 * 0.6)
                        col = (
                            clamp01(c_accent[0] * striation + 0.14),
                            clamp01(c_accent[1] * striation + 0.02),
                            clamp01(c_accent[2] * striation),
                        )
                        hv = 0.60 + 0.22 * striation
                        rough = 0.06
                    elif r < 0.49:
                        col = (0.12, 0.025, 0.025)
                        hv = 0.42
                        rough = 0.12
                        ao = 0.75
                    else:
                        vein1 = max(0.0, sin(ang * 16.0 + sin(r * 20.0) * 1.5) - 0.70) * 3.4
                        vein2 = max(0.0, sin(ang * 29.0 - sin(r * 26.0) * 1.1) - 0.78) * 3.8
                        vein = min(1.0, (vein1 + vein2 * 0.6) * min(1.0, (r - 0.48) * 2.6))
                        col = (
                            c_base[0] * (1.0 - vein * 0.22) + 0.68 * vein,
                            c_base[1] * (1.0 - vein * 0.78),
                            c_base[2] * (1.0 - vein * 0.80),
                        )
                        hv = 0.48 + 0.36 * vein + 0.06 * n2
                        rough = 0.16 + 0.08 * vein
                elif pattern == "spots":
                    spot = 0.0
                    for su, sv, sr in (
                        (0.20, 0.26, 0.11), (0.64, 0.20, 0.12), (0.44, 0.54, 0.13),
                        (0.82, 0.60, 0.10), (0.18, 0.74, 0.11), (0.58, 0.84, 0.09),
                        (0.84, 0.22, 0.07), (0.34, 0.86, 0.07),
                    ):
                        d = sqrt((u - su) ** 2 + (v - sv) ** 2)
                        if d < sr:
                            spot = max(spot, 1.0 - (d / sr) ** 2)
                    mix = min(1.0, spot * 1.2)
                    col = tuple((c_base[k] * (1.0 - mix) + c_accent[k] * mix) * (0.86 + 0.22 * t) for k in range(3))
                    hv = 0.35 + 0.50 * mix + 0.12 * t
                    rough = rough_base - mix * 0.18
                    ao = 0.78 + 0.22 * hv
                elif pattern == "gills":
                    ridge = 0.5 + 0.5 * sin(u * pi * 44.0 + n1 * 0.35)
                    col = tuple(c_base[k] * (0.55 + 0.45 * ridge) + c_accent[k] * (0.25 * (1.0 - ridge)) for k in range(3))
                    hv = 0.20 + 0.75 * ridge
                    rough = rough_base + (1.0 - ridge) * 0.18
                    ao = 0.55 + 0.45 * ridge
                elif pattern == "rings":
                    band = 0.5 + 0.5 * sin(v * pi * 18.0 + n1 * 0.55)
                    crevice = band ** 1.6
                    col = tuple(c_base[k] * (1.0 - crevice * 0.62) + c_accent[k] * (crevice * 0.62) for k in range(3))
                    hv = 0.22 + 0.68 * (1.0 - crevice) + 0.10 * n2
                    rough = rough_base + crevice * 0.24
                    ao = 0.62 + 0.38 * (1.0 - crevice)
                elif pattern == "scales":
                    s1 = sin((u + v) * pi * 18.0)
                    s2 = sin((u - v) * pi * 18.0)
                    sc = max(0.0, s1 * s2)
                    crown = sc ** 0.7
                    col = tuple((c_base[k] * (1.0 - crown * 0.72) + c_accent[k] * (crown * 0.72)) * (0.82 + 0.26 * crown) for k in range(3))
                    hv = 0.15 + 0.80 * crown + 0.05 * n2
                    rough = rough_base - 0.14 * crown + 0.18 * (1.0 - crown)
                    ao = 0.58 + 0.42 * crown
                elif pattern == "bubbles":
                    bub = 0.0
                    for su, sv, sr in (
                        (0.24, 0.34, 0.09), (0.72, 0.46, 0.12), (0.48, 0.72, 0.08),
                        (0.62, 0.20, 0.07), (0.18, 0.68, 0.07), (0.85, 0.78, 0.06),
                        (0.38, 0.22, 0.055), (0.78, 0.18, 0.05),
                    ):
                        d = sqrt((u - su) ** 2 + (v - sv) ** 2)
                        if d < sr:
                            bub = max(bub, sqrt(max(0.0, 1.0 - (d / sr) ** 2)))
                    mix = min(1.0, bub * 0.78 + t * 0.22)
                    col = tuple(c_base[k] * (1.0 - mix) + c_accent[k] * mix for k in range(3))
                    hv = 0.40 + 0.55 * bub + 0.05 * n1
                    rough = max(0.04, rough_base - 0.06 * bub)
                    ao = 0.86 + 0.14 * bub
                elif pattern == "chitin":
                    # Gleaming armored insect/arachnid carapace with subtle micro-pitting & seam ribs
                    rib = 0.5 + 0.5 * sin(v * pi * 12.0 + sin(u * pi * 6.0) * 0.3)
                    pit = 0.5 + 0.5 * n2
                    mix = 0.35 * rib + 0.25 * pit
                    col = tuple(c_base[k] * (1.0 - mix * 0.5) + c_accent[k] * (mix * 0.5) for k in range(3))
                    hv = 0.30 + 0.45 * rib + 0.20 * pit + 0.05 * n3
                    rough = rough_base + (1.0 - rib) * 0.18
                    ao = 0.72 + 0.28 * rib
                elif pattern == "spider_belly":
                    # Dark obsidian spider abdomen with subtle crimson dorsal hourglass/chevron markings & bristles
                    du = abs(u - 0.5) * 2.0
                    dv = (v - 0.5) * 2.0
                    # Hourglass / twin diamond dorsal mark centered on back
                    diamond = max(0.0, 1.0 - (du * 3.8 + abs(abs(dv) - 0.26) * 2.6))
                    stripe = max(0.0, sin(v * pi * 14.0) * (1.0 - du * 1.4)) * 0.45
                    mark = clamp01(diamond * 1.35 + stripe * 0.55)
                    bristle = 0.5 + 0.5 * sin(u * 95.0 + n1 * 3.0) * cos(v * 85.0)
                    col = tuple(
                        (c_base[k] * (1.0 - mark) + c_accent[k] * mark) * (0.80 + 0.28 * bristle)
                        for k in range(3)
                    )
                    hv = 0.32 + 0.34 * bristle + 0.24 * mark
                    rough = rough_base - 0.12 * mark + 0.14 * (1.0 - bristle)
                    ao = 0.70 + 0.30 * bristle
                else:
                    mix = clamp01(t)
                    col = tuple(c_base[k] * (1.0 - mix * 0.48) + c_accent[k] * (mix * 0.48) for k in range(3))
                    hv = mix
                    rough = rough_base + (0.5 - mix) * 0.20
                    ao = 0.78 + 0.22 * mix

                heights[row_off + px] = clamp01(hv) * (bump_str / 2.4)
                idx = (row_off + px) * 3
                albedo[idx] = max(0, min(255, int(clamp01(col[0]) * 255.0)))
                albedo[idx + 1] = max(0, min(255, int(clamp01(col[1]) * 255.0)))
                albedo[idx + 2] = max(0, min(255, int(clamp01(col[2]) * 255.0)))

                orm[idx] = max(0, min(255, int(clamp01(ao) * 255.0)))
                orm[idx + 1] = max(0, min(255, int(clamp01(rough) * 255.0)))
                orm[idx + 2] = max(0, min(255, int(clamp01(metal) * 255.0)))

    normal_rgb = _height_to_normal_rgb(heights, w, h, strength=3.4, wrap_x=False, wrap_y=False,
                                       flip_y_dx=False, band_width=size)
    return (
        encode_png_rgb(w, h, albedo),
        encode_png_rgb(w, h, normal_rgb),
        encode_png_rgb(w, h, orm),
    )


def generate_shared_organic_maps(raw_root: Path) -> None:
    """Write seamless tileable 256x256 organic hide/fur/scale detail + DirectX normal maps
    for Unreal's shared M_ABFP_CreaturePBRMaster material (works on both UV and non-UV meshes)."""
    size = 256
    detail = bytearray(size * size * 3)
    heights = [0.5] * (size * size)
    tau = 2.0 * math.pi
    sin = math.sin
    cos = math.cos
    inv = 1.0 / float(size)

    for y in range(size):
        v = y * inv * tau
        row = y * size
        for x in range(size):
            u = x * inv * tau
            # Strictly periodic multi-octave organic hide / fur / scale grain
            f1 = sin(u * 2.0 + v * 3.0) * cos(u * 3.0 - v * 2.0)
            f2 = sin(u * 5.0 - v * 4.0 + f1 * 0.9) * cos(u * 4.0 + v * 6.0)
            f3 = sin(u * 11.0 + v * 9.0) * cos(u * 9.0 - v * 13.0)
            f4 = sin(u * 21.0 - v * 19.0) * 0.5
            h_val = clamp01(0.5 + 0.24 * f1 + 0.16 * f2 + 0.11 * f3 + 0.06 * f4)
            heights[row + x] = h_val

            mottle = clamp01(0.5 + 0.34 * f1 + 0.18 * f2)
            rough_var = clamp01(0.55 - 0.32 * f2 + 0.18 * f3)
            ao = clamp01(0.68 + 0.32 * h_val)

            idx = (row + x) * 3
            detail[idx] = int(mottle * 255.0)
            detail[idx + 1] = int(rough_var * 255.0)
            detail[idx + 2] = int(ao * 255.0)

    # DirectX normal (+X right, -Y down) for direct UE TC_NORMALMAP import
    normal_dx = _height_to_normal_rgb(heights, size, size, strength=4.2, wrap_x=True, wrap_y=True, flip_y_dx=True)

    out_dir = raw_root / "creatures"
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "_shared_organic_detail.png").write_bytes(encode_png_rgb(size, size, detail))
    (out_dir / "_shared_organic_normal.png").write_bytes(encode_png_rgb(size, size, normal_dx))


class MeshBuilder:
    def __init__(self, num_bands: int):
        self.num_bands = max(1, num_bands)
        self.pos = []
        self.nor = []
        self.uv = []
        self.idx = []

    def _band_uv(self, band: int, u: float, v: float):
        u_c = 0.02 + 0.96 * max(0.0, min(1.0, u))
        return ((band + u_c) / self.num_bands, max(0.02, min(0.98, v)))

    def add_ellipsoid(self, center, radii, band: int, seg_u: int = 22, seg_v: int = 16,
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
            phi = fv * math.pi
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

    def add_tube(self, points, radii, band: int, radial_seg: int = 12, cap_ends: bool = True):
        n_pts = len(points)
        if n_pts < 2:
            return
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
            p0 = points[0]
            t0 = v_norm(v_sub(points[0], points[1]))
            c0 = len(self.pos)
            self.pos.append(p0)
            self.nor.append(t0)
            self.uv.append(self._band_uv(band, 0.5, 0.0))
            for j in range(radial_seg):
                self.idx.extend([c0, rings[0] + j, rings[0] + j + 1])
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
        stem_pts = []
        stem_r = []
        for i in range(9):
            t = i / 8.0
            sx = bx + math.sin(t * 1.4) * tilt_x * height * 0.45
            sy = by + t * height
            sz = bz + math.sin(t * 1.4) * tilt_z * height * 0.45
            stem_pts.append((sx, sy, sz))
            r = cap_r * (0.28 * (1.0 - t * 0.35) + (0.07 if i == 0 else 0.0))
            stem_r.append(r)
        self.add_tube(stem_pts, stem_r, stem_band, radial_seg=12)

        top = stem_pts[-1]
        def cap_deform(x, y, z, fu, fv):
            if y < 0.0:
                y *= 0.22
            wave = 1.0 + 0.065 * math.sin(fu * math.pi * 10.0) * math.sin(fv * math.pi)
            return (x * wave, y, z * wave)

        self.add_ellipsoid(
            (top[0], top[1] + cap_r * 0.12, top[2]),
            (cap_r, cap_r * 0.62, cap_r),
            cap_band,
            seg_u=24,
            seg_v=16,
            rot_xyz=(tilt_z * 0.35, 0.0, -tilt_x * 0.35),
            deform_fn=cap_deform,
        )
        self.add_ellipsoid(
            (top[0], top[1] - cap_r * 0.04, top[2]),
            (cap_r * 0.88, cap_r * 0.15, cap_r * 0.88),
            gill_band,
            seg_u=20,
            seg_v=8,
            rot_xyz=(tilt_z * 0.35, 0.0, -tilt_x * 0.35),
        )

    def add_jointed_leg(self, root, mid, foot, r_base: float, band: int, radial_seg: int = 8):
        pts = [
            root,
            v_add(v_scale(root, 0.5), v_scale(mid, 0.5)),
            mid,
            v_add(v_scale(mid, 0.5), v_scale(foot, 0.5)),
            foot,
        ]
        radii = [r_base, r_base * 0.88, r_base * 0.82, r_base * 0.56, r_base * 0.26]
        self.add_tube(pts, radii, band, radial_seg=radial_seg)

    def export_glb(self, out_path: Path, pbr_pngs: tuple[bytes, bytes, bytes], ground: bool = True,
                   normal_scale: float = 1.0):
        albedo_png, normal_png, orm_png = pbr_pngs
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
        alb_off, alb_len = add_chunk(albedo_png)
        nrm_off, nrm_len = add_chunk(normal_png)
        orm_off, orm_len = add_chunk(orm_png)

        while len(buf) % 4 != 0:
            buf.append(0)

        gltf = {
            "asset": {"version": "2.0", "generator": "ABFP High-Fidelity PBR Bestiary"},
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
                        "metallicRoughnessTexture": {"index": 2, "texCoord": 0},
                        "metallicFactor": 1.0,
                        "roughnessFactor": 1.0,
                    },
                    "normalTexture": {"index": 1, "texCoord": 0, "scale": float(normal_scale)},
                    "occlusionTexture": {"index": 2, "texCoord": 0, "strength": 0.65},
                }
            ],
            "textures": [
                {"source": 0},
                {"source": 1},
                {"source": 2},
            ],
            "images": [
                {"bufferView": 4, "mimeType": "image/png", "name": f"{out_path.stem}_Albedo"},
                {"bufferView": 5, "mimeType": "image/png", "name": f"{out_path.stem}_Normal"},
                {"bufferView": 6, "mimeType": "image/png", "name": f"{out_path.stem}_ORM"},
            ],
            "bufferViews": [
                {"buffer": 0, "byteOffset": pos_off, "byteLength": pos_len, "target": 34962},
                {"buffer": 0, "byteOffset": nor_off, "byteLength": nor_len, "target": 34962},
                {"buffer": 0, "byteOffset": uv_off, "byteLength": uv_len, "target": 34962},
                {"buffer": 0, "byteOffset": idx_off, "byteLength": idx_len, "target": 34963},
                {"buffer": 0, "byteOffset": alb_off, "byteLength": alb_len},
                {"buffer": 0, "byteOffset": nrm_off, "byteLength": nrm_len},
                {"buffer": 0, "byteOffset": orm_off, "byteLength": orm_len},
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
    tex = make_pbr_atlas_pngs([
        ((0.92, 0.87, 0.78), (0.88, 0.12, 0.06), "eye",    0.16, 0.0, 2.8),  # 0: Glossy veiny eyeball + iris
        ((0.46, 0.10, 0.14), (0.24, 0.04, 0.08), "rings",  0.46, 0.0, 2.6),  # 1: Fleshy optic nerve tendrils
    ])
    mb = MeshBuilder(2)

    def eye_cornea(x, y, z, fu, fv):
        # Subtle forward cornea bulge where +Z faces viewer
        if z > 0.34:
            bulge = (z - 0.34) * 0.28
            z += bulge
        return (x, y, z)

    mb.add_ellipsoid((0.0, 0.65, 0.0), (0.48, 0.48, 0.48), band=0, seg_u=36, seg_v=26,
                     deform_fn=eye_cornea, eye_front_uv=True)
    # Posterior fleshy nerve knot
    mb.add_ellipsoid((0.0, 0.56, -0.44), (0.20, 0.22, 0.18), band=1, seg_u=16, seg_v=12)
    # 9 trailing sinuous optic nerve tendrils
    for k in range(9):
        ang = (k / 9.0) * 2.0 * math.pi + 0.25
        pts = []
        rads = []
        for s in range(11):
            t = s / 10.0
            r_out = 0.10 + 0.25 * math.sin(t * 2.3 + k * 0.4) + (0.14 if k > 0 else 0.0)
            px = math.cos(ang + t * 0.95) * r_out
            py = 0.30 - t * 0.76
            pz = -0.18 - t * 0.34 + math.sin(ang + t * 1.5) * 0.11
            pts.append((px, py, pz))
            rads.append(0.056 * (1.0 - t * 0.78))
        mb.add_tube(pts, rads, band=1, radial_seg=9)
    mb.export_glb(out_path, tex)


def gen_mushroom_patch(out_path: Path):
    tex = make_pbr_atlas_pngs([
        ((0.60, 0.14, 0.10), (0.95, 0.90, 0.74), "spots",  0.56, 0.0, 3.0),  # 0: Crimson spotted toadstool cap
        ((0.80, 0.74, 0.58), (0.58, 0.50, 0.36), "mottle", 0.78, 0.0, 2.0),  # 1: Fibrous cream stem
        ((0.46, 0.34, 0.22), (0.24, 0.16, 0.10), "gills",  0.80, 0.0, 3.6),  # 2: Radial underside gills
        ((0.18, 0.26, 0.12), (0.11, 0.15, 0.07), "mottle", 0.90, 0.0, 3.0),  # 3: Mossy humus base
        ((0.74, 0.50, 0.14), (0.96, 0.88, 0.56), "spots",  0.54, 0.0, 3.0),  # 4: Ochre-spotted variant cap
    ])
    mb = MeshBuilder(5)
    mb.add_ellipsoid((0.0, 0.08, 0.0), (0.84, 0.15, 0.80), band=3, seg_u=22, seg_v=10)
    specs = [
        ((0.02, 0.10, 0.02),   0.74, 0.45,  0.10,  0.08, 0),
        ((-0.40, 0.08, 0.22),  0.50, 0.31, -0.32,  0.22, 4),
        ((0.38, 0.08, 0.18),   0.56, 0.33,  0.28,  0.18, 0),
        ((-0.24, 0.08, -0.34), 0.45, 0.27, -0.22, -0.28, 4),
        ((0.28, 0.08, -0.30),  0.38, 0.23,  0.25, -0.26, 0),
        ((-0.05, 0.08, 0.44),  0.30, 0.18, -0.10,  0.35, 4),
    ]
    for pos, h, r, tx, tz, cband in specs:
        mb.add_mushroom(pos, h, r, tx, tz, stem_band=1, cap_band=cband, gill_band=2)
    mb.export_glb(out_path, tex)


def gen_fungal_mold(out_path: Path):
    tex = make_pbr_atlas_pngs([
        ((0.34, 0.42, 0.16), (0.58, 0.64, 0.22), "mottle",  0.86, 0.0, 3.2),  # 0: Fuzzy olive-ochre cushions
        ((0.20, 0.23, 0.14), (0.12, 0.14, 0.08), "mottle",  0.92, 0.0, 2.8),  # 1: Dark mycelial mat
        ((0.82, 0.72, 0.22), (0.98, 0.88, 0.40), "bubbles", 0.28, 0.0, 2.6),  # 2: Glistening spore pustules
    ])
    mb = MeshBuilder(3)
    rng = random.Random(42)
    mb.add_ellipsoid((0.0, 0.06, 0.0), (0.96, 0.11, 0.94), band=1, seg_u=24, seg_v=10)
    for k in range(16):
        ang = k * 2.39996
        dist = 0.08 + 0.58 * math.sqrt((k + 0.5) / 16.0)
        rx = 0.19 + rng.uniform(0.0, 0.16)
        ry = 0.18 + (0.28 if k < 4 else rng.uniform(0.0, 0.18))
        rz = 0.19 + rng.uniform(0.0, 0.16)
        px = math.cos(ang) * dist
        pz = math.sin(ang) * dist
        mb.add_ellipsoid((px, ry * 0.65, pz), (rx, ry, rz), band=0, seg_u=16, seg_v=12)
        if k % 2 == 0:
            mb.add_ellipsoid((px, ry * 1.24, pz), (rx * 0.42, ry * 0.42, rz * 0.42), band=2, seg_u=12, seg_v=10)
    mb.export_glb(out_path, tex)


def gen_giant_ant(out_path: Path):
    tex = make_pbr_atlas_pngs([
        ((0.26, 0.07, 0.03), (0.48, 0.16, 0.06), "chitin", 0.24, 0.14, 2.8),  # 0: Glossy armored chitin carapace
        ((0.14, 0.04, 0.02), (0.28, 0.10, 0.04), "rings",  0.34, 0.08, 2.2),  # 1: Jointed legs & mandibles
        ((0.92, 0.22, 0.06), (0.28, 0.04, 0.02), "spots",  0.08, 0.10, 2.0),  # 2: Glossy compound eyes
    ])
    mb = MeshBuilder(3)
    mb.add_ellipsoid((0.0, 0.46, -0.68), (0.35, 0.31, 0.50), band=0, seg_u=24, seg_v=18, rot_xyz=(-0.18, 0.0, 0.0))
    mb.add_ellipsoid((0.0, 0.40, -0.20), (0.14, 0.13, 0.18), band=0, seg_u=14, seg_v=10)
    mb.add_ellipsoid((0.0, 0.48,  0.12), (0.23, 0.22, 0.31), band=0, seg_u=22, seg_v=16)
    mb.add_ellipsoid((0.0, 0.56,  0.54), (0.25, 0.22, 0.25), band=0, seg_u=22, seg_v=16, rot_xyz=(0.22, 0.0, 0.0))
    for side in (-1.0, 1.0):
        mb.add_ellipsoid((side * 0.20, 0.62, 0.62), (0.070, 0.068, 0.080), band=2, seg_u=14, seg_v=10)
        mb.add_tube(
            [(side * 0.10, 0.48, 0.72), (side * 0.18, 0.44, 0.89), (side * 0.06, 0.41, 1.02)],
            [0.052, 0.038, 0.014], band=1, radial_seg=8,
        )
        mb.add_tube(
            [(side * 0.09, 0.64, 0.72), (side * 0.26, 0.86, 0.95), (side * 0.42, 0.74, 1.20)],
            [0.024, 0.018, 0.009], band=1, radial_seg=7,
        )
        for z_root, z_foot, x_mid, x_foot in (
            ( 0.26,  0.58, 0.54, 0.80),
            ( 0.10,  0.08, 0.64, 0.94),
            (-0.08, -0.58, 0.60, 0.88),
        ):
            mb.add_jointed_leg(
                (side * 0.16, 0.40, z_root),
                (side * x_mid, 0.54, (z_root + z_foot) * 0.5),
                (side * x_foot, 0.0, z_foot),
                0.048, band=1, radial_seg=8,
            )
    mb.export_glb(out_path, tex)


def gen_giant_centipede(out_path: Path):
    tex = make_pbr_atlas_pngs([
        ((0.48, 0.32, 0.12), (0.16, 0.10, 0.05), "chitin", 0.22, 0.18, 3.0),  # 0: Banded armored carapace
        ((0.28, 0.15, 0.06), (0.12, 0.06, 0.03), "rings",  0.34, 0.10, 2.2),  # 1: Legs & mandibles
        ((0.92, 0.18, 0.06), (0.30, 0.04, 0.02), "spots",  0.08, 0.05, 1.8),  # 2: Gleaming eyes
    ])
    mb = MeshBuilder(3)
    n_seg = 12
    seg_centers = []
    for i in range(n_seg):
        t = i / float(n_seg - 1)
        z = 1.20 - t * 2.40
        x = math.sin(t * math.pi * 1.65) * 0.44
        y = 0.22 + (0.09 if i == 0 else 0.0)
        seg_centers.append((x, y, z))
        width = 0.23 * (0.78 + 0.22 * math.sin(t * math.pi))
        mb.add_ellipsoid((x, y, z), (width, 0.155, 0.195), band=0, seg_u=18, seg_v=12)
        if i > 0:
            dx = seg_centers[i - 1][0] - x
            dz = seg_centers[i - 1][2] - z
            tang = v_norm((dx, 0.0, dz))
            side_v = (-tang[2], 0.0, tang[0])
            for side in (-1.0, 1.0):
                root = (x + side_v[0] * side * width * 0.8, y - 0.02, z + side_v[2] * side * width * 0.8)
                mid = (x + side_v[0] * side * (width + 0.22), y + 0.09, z + side_v[2] * side * (width + 0.22))
                foot = (x + side_v[0] * side * (width + 0.39), 0.0, z + side_v[2] * side * (width + 0.39))
                mb.add_jointed_leg(root, mid, foot, 0.034, band=1, radial_seg=7)

    hx, hy, hz = seg_centers[0]
    for side in (-1.0, 1.0):
        mb.add_ellipsoid((hx + side * 0.14, hy + 0.08, hz + 0.13), (0.048, 0.048, 0.048), band=2, seg_u=12, seg_v=8)
        mb.add_tube(
            [(hx + side * 0.09, hy - 0.02, hz + 0.15), (hx + side * 0.19, hy - 0.03, hz + 0.33), (hx + side * 0.07, hy - 0.03, hz + 0.44)],
            [0.040, 0.030, 0.012], band=1, radial_seg=8,
        )
        mb.add_tube(
            [(hx + side * 0.08, hy + 0.06, hz + 0.16), (hx + side * 0.28, hy + 0.23, hz + 0.47), (hx + side * 0.47, hy + 0.18, hz + 0.74)],
            [0.022, 0.015, 0.008], band=1, radial_seg=7,
        )
    mb.export_glb(out_path, tex)


def gen_worm_mass(out_path: Path):
    tex = make_pbr_atlas_pngs([
        ((0.76, 0.60, 0.52), (0.48, 0.32, 0.28), "rings",  0.34, 0.0, 3.0),  # 0: Slimy pale ringed worm
        ((0.62, 0.54, 0.38), (0.36, 0.30, 0.20), "rings",  0.38, 0.0, 3.0),  # 1: Earth-ochre ringed worm
        ((0.26, 0.20, 0.16), (0.14, 0.10, 0.08), "mottle", 0.76, 0.0, 2.8),  # 2: Damp organic heap
    ])
    mb = MeshBuilder(3)
    mb.add_ellipsoid((0.0, 0.16, 0.0), (0.70, 0.21, 0.68), band=2, seg_u=22, seg_v=12)
    for k in range(10):
        ang = (k / 10.0) * 2.0 * math.pi
        pts = []
        rads = []
        for s in range(13):
            t = s / 12.0
            r = 0.58 * (1.0 - t * 0.55) + 0.14 * math.sin(t * 5.2 + k)
            a = ang + t * 2.4 + math.sin(t * 4.0 + k * 1.3) * 0.35
            px = math.cos(a) * r
            pz = math.sin(a) * r
            py = 0.08 + math.sin(t * math.pi) * (0.50 + (k % 3) * 0.14) + t * 0.22
            pts.append((px, py, pz))
            seg_bulge = 1.0 + 0.15 * math.sin(t * math.pi * 12.0)
            rads.append((0.076 - t * 0.032) * seg_bulge)
        mb.add_tube(pts, rads, band=(k % 2), radial_seg=10)
    mb.export_glb(out_path, tex)


def gen_louse_beetle(out_path: Path):
    tex = make_pbr_atlas_pngs([
        ((0.58, 0.52, 0.40), (0.28, 0.24, 0.18), "chitin", 0.26, 0.16, 3.0),  # 0: Ribbed armored shell
        ((0.22, 0.18, 0.14), (0.10, 0.08, 0.06), "rings",  0.38, 0.08, 2.2),  # 1: Legs, head & horns
    ])
    mb = MeshBuilder(2)
    def shell_deform(x, y, z, fu, fv):
        if y < 0.0:
            y *= 0.25
        # Subtle dorsal elytra seam ridge down center
        seam = 1.0 - 0.06 * math.exp(-((x / 0.04) ** 2))
        return (x, y * seam, z)
    mb.add_ellipsoid((0.0, 0.25, -0.08), (0.44, 0.31, 0.56), band=0, seg_u=26, seg_v=18, deform_fn=shell_deform)
    mb.add_ellipsoid((0.0, 0.23,  0.48), (0.22, 0.17, 0.22), band=1, seg_u=18, seg_v=12)
    for side in (-1.0, 1.0):
        # Forward horned mandibles
        mb.add_tube(
            [(side * 0.09, 0.22, 0.62), (side * 0.18, 0.25, 0.78), (side * 0.08, 0.26, 0.90)],
            [0.034, 0.024, 0.010], band=1, radial_seg=7,
        )
        for zr, zf, xm, xf in ((0.26, 0.44, 0.50, 0.66), (0.0, 0.02, 0.58, 0.74), (-0.28, -0.46, 0.52, 0.68)):
            mb.add_jointed_leg((side * 0.26, 0.20, zr), (side * xm, 0.27, (zr + zf) * 0.5), (side * xf, 0.0, zf), 0.040, band=1, radial_seg=8)
    mb.export_glb(out_path, tex)


def gen_dungeon_slime(out_path: Path):
    tex = make_pbr_atlas_pngs([
        ((0.10, 0.56, 0.30), (0.48, 0.96, 0.64), "bubbles", 0.07, 0.04, 3.0),  # 0: Wet glossy emerald slime
        ((0.05, 0.24, 0.14), (0.16, 0.46, 0.26), "mottle",  0.14, 0.0,  2.2),  # 1: Dense dark nucleus
    ])
    mb = MeshBuilder(2)
    def slime_deform(x, y, z, fu, fv):
        if y < -0.35:
            y = -0.35 + (y + 0.35) * 0.12
        skirt = max(0.0, (-y) / 0.55)
        lobe = 1.0 + 0.19 * skirt * math.sin(fu * math.pi * 10.0) + 0.08 * math.cos(fu * math.pi * 6.0 + fv * 5.0)
        return (x * lobe, y, z * lobe)

    mb.add_ellipsoid((0.0, 0.42, 0.0), (0.58, 0.52, 0.58), band=0, seg_u=32, seg_v=24, deform_fn=slime_deform)
    for k in range(6):
        ang = k * (2.0 * math.pi / 6.0) + 0.2
        mb.add_ellipsoid(
            (math.cos(ang) * 0.48, 0.12, math.sin(ang) * 0.48),
            (0.22, 0.14, 0.22), band=0, seg_u=16, seg_v=10,
        )
    mb.export_glb(out_path, tex)


def gen_giant_frog(out_path: Path):
    tex = make_pbr_atlas_pngs([
        ((0.16, 0.44, 0.14), (0.34, 0.56, 0.20), "spots",  0.26, 0.0, 3.2),  # 0: Wet warty green frog skin
        ((0.68, 0.72, 0.46), (0.46, 0.52, 0.30), "mottle", 0.38, 0.0, 2.0),  # 1: Pale damp belly
        ((0.96, 0.76, 0.12), (0.12, 0.06, 0.03), "eye",    0.06, 0.0, 2.2),  # 2: Glossy golden frog eyes
    ])
    mb = MeshBuilder(3)
    mb.add_ellipsoid((0.0, 0.36, 0.0), (0.45, 0.31, 0.54), band=0, seg_u=26, seg_v=18, rot_xyz=(-0.38, 0.0, 0.0))
    mb.add_ellipsoid((0.0, 0.26, 0.10), (0.37, 0.21, 0.43), band=1, seg_u=20, seg_v=14, rot_xyz=(-0.38, 0.0, 0.0))
    mb.add_ellipsoid((0.0, 0.50, 0.46), (0.37, 0.21, 0.31), band=0, seg_u=22, seg_v=16)
    for side in (-1.0, 1.0):
        mb.add_ellipsoid((side * 0.20, 0.69, 0.50), (0.105, 0.105, 0.105), band=2, seg_u=18, seg_v=14, eye_front_uv=True)
        mb.add_ellipsoid((side * 0.43, 0.24, -0.26), (0.17, 0.19, 0.31), band=0, seg_u=16, seg_v=12, rot_xyz=(0.35, side * 0.25, 0.0))
        mb.add_tube(
            [(side * 0.46, 0.16, -0.34), (side * 0.53, 0.06, -0.02), (side * 0.55, 0.04, 0.20)],
            [0.092, 0.072, 0.056], band=0, radial_seg=9,
        )
        mb.add_jointed_leg((side * 0.30, 0.36, 0.32), (side * 0.38, 0.22, 0.44), (side * 0.32, 0.03, 0.56), 0.078, band=0, radial_seg=9)
    mb.export_glb(out_path, tex)


def gen_reared_cobra(out_path: Path):
    tex = make_pbr_atlas_pngs([
        ((0.18, 0.40, 0.16), (0.50, 0.64, 0.22), "scales", 0.28, 0.06, 3.6),  # 0: Raised diamond cobra scales
        ((0.78, 0.68, 0.40), (0.52, 0.42, 0.22), "rings",  0.36, 0.0,  2.8),  # 1: Ribbed hood belly plates
        ((0.96, 0.72, 0.08), (0.10, 0.04, 0.02), "eye",    0.06, 0.0,  2.0),  # 2: Slit golden eyes
        ((0.94, 0.90, 0.82), (0.76, 0.14, 0.14), "mottle", 0.16, 0.0,  1.2),  # 3: Ivory fangs & crimson tongue
    ])
    mb = MeshBuilder(4)
    pts = []
    rads = []
    for i in range(26):
        t = i / 25.0
        ang = t * math.pi * 2.65
        rad_coil = 0.60 - t * 0.30
        px = math.cos(ang) * rad_coil
        pz = math.sin(ang) * rad_coil
        py = 0.09 + t * 0.15
        pts.append((px, py, pz))
        rads.append(0.048 + t * 0.074)
    neck_pts = [
        (0.18, 0.28, -0.10),
        (0.08, 0.52, -0.18),
        (0.00, 0.78, -0.12),
        (0.00, 1.02,  0.04),
        (0.00, 1.18,  0.22),
        (0.00, 1.14,  0.44),
    ]
    neck_rads = [
        (0.11, 0.11),
        (0.13, 0.10),
        (0.27, 0.08),  # Wide sculpted flared cobra hood
        (0.29, 0.085),
        (0.14, 0.105),
        (0.065, 0.055),
    ]
    pts.extend(neck_pts)
    rads.extend(neck_rads)
    mb.add_tube(pts, rads, band=0, radial_seg=14)
    mb.add_ellipsoid((0.0, 0.90, 0.04), (0.26, 0.24, 0.075), band=1, seg_u=20, seg_v=14, rot_xyz=(-0.25, 0.0, 0.0))
    for side in (-1.0, 1.0):
        # Protruding eyes
        mb.add_ellipsoid((side * 0.095, 1.21, 0.30), (0.036, 0.036, 0.036), band=2, seg_u=12, seg_v=10, eye_front_uv=True)
        # Curved venomous ivory fangs
        mb.add_tube(
            [(side * 0.042, 1.11, 0.40), (side * 0.042, 1.05, 0.42), (side * 0.038, 1.00, 0.40)],
            [0.014, 0.010, 0.003], band=3, radial_seg=6,
        )
        # Forked tongue tips
        mb.add_tube(
            [(0.0, 1.11, 0.44), (0.0, 1.09, 0.54), (side * 0.035, 1.07, 0.62)],
            [0.009, 0.007, 0.003], band=3, radial_seg=5,
        )
    mb.export_glb(out_path, tex)


def gen_gelatinous_cube(out_path: Path):
    tex = make_pbr_atlas_pngs([
        ((0.22, 0.74, 0.66), (0.64, 0.98, 0.92), "bubbles", 0.06, 0.05, 2.8),  # 0: Glossy rippling aqua gel
        ((0.86, 0.80, 0.68), (0.58, 0.52, 0.42), "mottle",  0.62, 0.0,  2.2),  # 1: Trapped bleached bones/skull
    ])
    mb = MeshBuilder(2)
    def cube_deform(x, y, z, fu, fv):
        m = (abs(x) ** 6 + abs(y) ** 6 + abs(z) ** 6) ** (1.0 / 6.0)
        if m > 1e-5:
            s = 0.66 / m
            x, y, z = x * s, y * s, z * s
        ripple = 1.0 + 0.035 * math.sin(fu * math.pi * 8.0 + fv * math.pi * 6.0)
        return (x * ripple, y * ripple, z * ripple)

    mb.add_ellipsoid((0.0, 0.66, 0.0), (0.66, 0.66, 0.66), band=0, seg_u=30, seg_v=22, deform_fn=cube_deform)
    # Partially embedded bleached skull & bone fragments emerging near the gelatinous walls
    mb.add_ellipsoid((0.18, 0.82, 0.58), (0.16, 0.18, 0.14), band=1, seg_u=16, seg_v=12, rot_xyz=(0.2, -0.3, 0.15))
    mb.add_tube([(-0.48, 0.42, 0.56), (-0.18, 0.56, 0.60)], [0.042, 0.038], band=1, radial_seg=8)
    mb.add_tube([(0.56, 0.36, -0.22), (0.60, 0.62, 0.16)], [0.038, 0.040], band=1, radial_seg=8)
    mb.export_glb(out_path, tex)


def gen_cave_spider(out_path: Path):
    """Sculpted high-detail Cave Spider: armored cephalothorax, hourglass-patterned
    bulbous abdomen, 8 articulated 3-joint legs, pedipalps, venomous fangs, and 8 gleaming crimson eyes."""
    tex = make_pbr_atlas_pngs([
        ((0.09, 0.06, 0.10), (0.78, 0.14, 0.08), "spider_belly", 0.32, 0.10, 3.2),  # 0: Dark obsidian abdomen + crimson mark
        ((0.12, 0.09, 0.11), (0.24, 0.17, 0.19), "chitin",       0.26, 0.14, 2.8),  # 1: Armored cephalothorax & legs
        ((0.96, 0.10, 0.06), (0.35, 0.02, 0.02), "eye",          0.04, 0.05, 1.8),  # 2: Gleaming ruby spider eyes
        ((0.16, 0.04, 0.04), (0.62, 0.08, 0.06), "rings",        0.18, 0.16, 2.2),  # 3: Venomous curved fangs
    ])
    mb = MeshBuilder(4)

    # Large bulbous patterned abdomen in back (-Z)
    def abd_deform(x, y, z, fu, fv):
        # Taper gently toward spinnerets at back
        taper = 1.0 - max(0.0, -z) * 0.25
        return (x * taper, y * (0.94 + 0.06 * math.sin(fv * math.pi * 8.0)), z)
    mb.add_ellipsoid((0.0, 0.46, -0.46), (0.42, 0.36, 0.54), band=0, seg_u=28, seg_v=20,
                     rot_xyz=(-0.14, 0.0, 0.0), deform_fn=abd_deform)
    # Spinnerets at posterior tip
    for side in (-1.0, 1.0):
        mb.add_tube([(side * 0.07, 0.36, -0.94), (side * 0.10, 0.34, -1.08)], [0.036, 0.016], band=1, radial_seg=6)

    # Narrow waist (pedicel) + armored cephalothorax forward (+Z)
    mb.add_ellipsoid((0.0, 0.36, 0.05), (0.14, 0.12, 0.14), band=1, seg_u=14, seg_v=10)
    mb.add_ellipsoid((0.0, 0.38, 0.34), (0.30, 0.22, 0.34), band=1, seg_u=24, seg_v=16, rot_xyz=(0.12, 0.0, 0.0))

    # 8 gleaming crimson eyes arranged on the front brow
    eye_specs = [
        (0.075, 0.46, 0.63, 0.048),   # Primary median pair (large)
        (0.155, 0.45, 0.58, 0.034),   # Secondary lateral pair
        (0.048, 0.53, 0.59, 0.028),   # Upper median pair
        (0.135, 0.51, 0.53, 0.026),   # Upper lateral pair
    ]
    for ex, ey, ez, er in eye_specs:
        for side in (-1.0, 1.0):
            mb.add_ellipsoid((side * ex, ey, ez), (er, er, er), band=2, seg_u=12, seg_v=10, eye_front_uv=True)

    # Chelicerae + curved venomous fangs + forward pedipalps
    for side in (-1.0, 1.0):
        # Fangs curving downward and slightly inward
        mb.add_tube(
            [
                (side * 0.09, 0.33, 0.64),
                (side * 0.11, 0.24, 0.75),
                (side * 0.06, 0.14, 0.72),
            ],
            [0.058, 0.038, 0.010], band=3, radial_seg=8,
        )
        # Sensory pedipalps flanking fangs
        mb.add_tube(
            [
                (side * 0.18, 0.34, 0.58),
                (side * 0.32, 0.38, 0.80),
                (side * 0.26, 0.16, 0.94),
            ],
            [0.042, 0.034, 0.020], band=1, radial_seg=7,
        )

        # 4 pairs of tall menacingly arched 3-joint spider legs planted firmly at Y = 0
        leg_defs = [
            # (z_root, knee_x, knee_y, knee_z, ankle_x, ankle_y, ankle_z, foot_x, foot_z)
            (0.46, 0.56, 0.78,  0.76, 0.90, 0.44,  0.96, 1.08,  1.12),  # Front leg I
            (0.36, 0.66, 0.74,  0.42, 1.02, 0.42,  0.48, 1.20,  0.52),  # Mid-front leg II
            (0.24, 0.66, 0.72, -0.02, 1.00, 0.40, -0.14, 1.18, -0.26),  # Mid-rear leg III
            (0.14, 0.58, 0.76, -0.46, 0.90, 0.44, -0.78, 1.08, -1.04),  # Rear leg IV
        ]
        for zr, kx, ky, kz, ax, ay, az, fx, fz in leg_defs:
            pts = [
                (side * 0.22, 0.34, zr),
                (side * (0.22 + kx) * 0.5, (0.34 + ky) * 0.54, (zr + kz) * 0.5),
                (side * kx, ky, kz),
                (side * ax, ay, az),
                (side * fx, 0.0, fz),
            ]
            rads = [0.056, 0.050, 0.046, 0.034, 0.014]
            mb.add_tube(pts, rads, band=1, radial_seg=8)

    mb.export_glb(out_path, tex, normal_scale=1.15)


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
    "creatures/abfp_cave_spider.glb": gen_cave_spider,
}


def generate_all(raw_root: Path) -> int:
    generate_shared_organic_maps(raw_root)
    count = 0
    for rel, fn in GENERATORS.items():
        dst = raw_root / rel
        fn(dst)
        count += 1
    return count


if __name__ == "__main__":
    repo_root = Path(__file__).resolve().parent.parent
    n = generate_all(repo_root / "assets" / "raw")
    print(f"Generated {n} high-fidelity PBR Angband dungeon creature GLBs + shared organic detail maps in {repo_root / 'assets' / 'raw' / 'creatures'}")
