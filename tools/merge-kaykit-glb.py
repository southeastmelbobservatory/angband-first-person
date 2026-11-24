#!/usr/bin/env python3
"""Merge multi-part KayKit character GLBs (ArmLeft, ArmRight, Body, Head, LegLeft, LegRight, Cape/Cloak/Helmet)
into a single unified static GLB with world transforms baked and embedded texture preserved.

Prevents UE 5.8 Interchange from splitting KayKit characters into separate detached limb assets.
Uses Python 3 standard library only (no external dependencies).
"""

import json
import math
import struct
import sys
from pathlib import Path

SKIP_NODE_KEYWORDS = (
    "crossbow",
    "shield",
    "sword",
    "dagger",
    "knife",
    "axe",
    "staff",
    "wand",
    "spellbook",
    "throwable",
    "quiver",
    "arrow",
    "bow",
    "mug",
    "blade",
)


def mat4_identity():
    return [
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0,
    ]


def mat4_mul(a, b):
    # Column-major 4x4 matrix multiplication (glTF convention)
    out = [0.0] * 16
    for col in range(4):
        for row in range(4):
            out[col * 4 + row] = (
                a[0 * 4 + row] * b[col * 4 + 0]
                + a[1 * 4 + row] * b[col * 4 + 1]
                + a[2 * 4 + row] * b[col * 4 + 2]
                + a[3 * 4 + row] * b[col * 4 + 3]
            )
    return out


def mat4_from_trs(t, r, s):
    tx, ty, tz = t
    qx, qy, qz, qw = r
    sx, sy, sz = s
    xx, yy, zz = qx * qx, qy * qy, qz * qz
    xy, xz, yz = qx * qy, qx * qz, qy * qz
    wx, wy, wz = qw * qx, qw * qy, qw * qz
    return [
        (1.0 - 2.0 * (yy + zz)) * sx, (2.0 * (xy + wz)) * sx, (2.0 * (xz - wy)) * sx, 0.0,
        (2.0 * (xy - wz)) * sy, (1.0 - 2.0 * (xx + zz)) * sy, (2.0 * (yz + wx)) * sy, 0.0,
        (2.0 * (xz + wy)) * sz, (2.0 * (yz - wx)) * sz, (1.0 - 2.0 * (xx + yy)) * sz, 0.0,
        tx, ty, tz, 1.0,
    ]


def node_local_mat(node):
    if "matrix" in node:
        return list(node["matrix"])
    t = node.get("translation", [0.0, 0.0, 0.0])
    r = node.get("rotation", [0.0, 0.0, 0.0, 1.0])
    s = node.get("scale", [1.0, 1.0, 1.0])
    return mat4_from_trs(t, r, s)


def transform_point(m, x, y, z):
    return (
        m[0] * x + m[4] * y + m[8] * z + m[12],
        m[1] * x + m[5] * y + m[9] * z + m[13],
        m[2] * x + m[6] * y + m[10] * z + m[14],
    )


def transform_normal(m, nx, ny, nz):
    x = m[0] * nx + m[4] * ny + m[8] * nz
    y = m[1] * nx + m[5] * ny + m[9] * nz
    z = m[2] * nx + m[6] * ny + m[10] * nz
    length = math.sqrt(x * x + y * y + z * z)
    if length > 1e-8:
        return (x / length, y / length, z / length)
    return (0.0, 1.0, 0.0)


def read_accessor(gltf, bin_data, acc_idx):
    acc = gltf["accessors"][acc_idx]
    bv = gltf["bufferViews"][acc["bufferView"]]
    count = acc["count"]
    comp_type = acc["componentType"]
    acc_type = acc["type"]
    fmt_map = {5120: "b", 5121: "B", 5122: "h", 5123: "H", 5125: "I", 5126: "f"}
    size_map = {5120: 1, 5121: 1, 5122: 2, 5123: 2, 5125: 4, 5126: 4}
    num_map = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}
    n_comp = num_map[acc_type]
    c_fmt = fmt_map[comp_type]
    elem_size = size_map[comp_type] * n_comp
    stride = bv.get("byteStride", elem_size)
    offset = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    s_obj = struct.Struct("<" + c_fmt * n_comp)
    out = []
    for i in range(count):
        vals = s_obj.unpack_from(bin_data, offset + i * stride)
        out.append(vals[0] if n_comp == 1 else vals)
    return out


def merge_kaykit_glb(src_path: Path, dst_path: Path) -> bool:
    data = src_path.read_bytes()
    if len(data) < 28 or data[:4] != b"glTF":
        return False
    json_len = struct.unpack_from("<I", data, 12)[0]
    gltf = json.loads(data[20 : 20 + json_len].decode("utf-8"))
    bin_off = 20 + json_len + 8
    bin_len = struct.unpack_from("<I", data, 20 + json_len)[0]
    bin_data = data[bin_off : bin_off + bin_len]

    nodes = gltf.get("nodes", [])
    scenes = gltf.get("scenes", [])
    root_nodes = scenes[gltf.get("scene", 0)].get("nodes", []) if scenes else list(range(len(nodes)))

    world_mats = {}

    def walk(idx, parent_m):
        m = mat4_mul(parent_m, node_local_mat(nodes[idx]))
        world_mats[idx] = m
        for ch in nodes[idx].get("children", []):
            walk(ch, m)

    for r in root_nodes:
        walk(r, mat4_identity())

    all_pos = []
    all_nor = []
    all_uv = []
    all_idx = []

    for idx, node in enumerate(nodes):
        if "mesh" not in node:
            continue
        name_l = (node.get("name") or "").lower()
        # Keep skinned body parts + worn head/cape/cloak/helmet; skip detached loose weapon props
        if any(bad in name_l for bad in SKIP_NODE_KEYWORDS):
            continue
        m = world_mats.get(idx, mat4_identity())
        mesh = gltf["meshes"][node["mesh"]]
        for prim in mesh.get("primitives", []):
            attrs = prim.get("attributes", {})
            if "POSITION" not in attrs:
                continue
            pos = read_accessor(gltf, bin_data, attrs["POSITION"])
            nor = read_accessor(gltf, bin_data, attrs["NORMAL"]) if "NORMAL" in attrs else [(0.0, 1.0, 0.0)] * len(pos)
            uv = read_accessor(gltf, bin_data, attrs["TEXCOORD_0"]) if "TEXCOORD_0" in attrs else [(0.0, 0.0)] * len(pos)
            if "indices" in prim:
                indices = read_accessor(gltf, bin_data, prim["indices"])
            else:
                indices = list(range(len(pos)))

            base_v = len(all_pos)
            for p, n, t in zip(pos, nor, uv):
                all_pos.append(transform_point(m, p[0], p[1], p[2]))
                all_nor.append(transform_normal(m, n[0], n[1], n[2]))
                all_uv.append((float(t[0]), float(t[1])))
            for k in indices:
                all_idx.append(base_v + int(k))

    if not all_pos or not all_idx:
        return False

    # Re-ground bottom of merged mesh cleanly to Y = 0 (glTF Y-up) and center X/Z
    xs = [p[0] for p in all_pos]
    ys = [p[1] for p in all_pos]
    zs = [p[2] for p in all_pos]
    cx = 0.5 * (min(xs) + max(xs))
    cz = 0.5 * (min(zs) + max(zs))
    min_y = min(ys)
    all_pos = [(p[0] - cx, p[1] - min_y, p[2] - cz) for p in all_pos]

    pmin = [min(p[k] for p in all_pos) for k in range(3)]
    pmax = [max(p[k] for p in all_pos) for k in range(3)]

    # Extract embedded image (first PNG/JPEG) if present
    img_bytes = None
    img_mime = "image/png"
    if gltf.get("images"):
        im0 = gltf["images"][0]
        if "bufferView" in im0:
            ibv = gltf["bufferViews"][im0["bufferView"]]
            ioff = ibv.get("byteOffset", 0)
            img_bytes = bin_data[ioff : ioff + ibv["byteLength"]]
            img_mime = im0.get("mimeType", "image/png")

    # Pack new single-buffer GLB
    buf = bytearray()

    def add_chunk(raw_bytes):
        while len(buf) % 4 != 0:
            buf.append(0)
        off = len(buf)
        buf.extend(raw_bytes)
        return off, len(raw_bytes)

    pos_raw = struct.pack(f"<{len(all_pos) * 3}f", *[v for p in all_pos for v in p])
    nor_raw = struct.pack(f"<{len(all_nor) * 3}f", *[v for n in all_nor for v in n])
    uv_raw = struct.pack(f"<{len(all_uv) * 2}f", *[v for t in all_uv for v in t])
    use_u16 = len(all_pos) < 65535
    idx_comp_type = 5123 if use_u16 else 5125
    idx_raw = struct.pack(f"<{len(all_idx)}{'H' if use_u16 else 'I'}", *all_idx)

    pos_off, pos_len = add_chunk(pos_raw)
    nor_off, nor_len = add_chunk(nor_raw)
    uv_off, uv_len = add_chunk(uv_raw)
    idx_off, idx_len = add_chunk(idx_raw)

    buffer_views = [
        {"buffer": 0, "byteOffset": pos_off, "byteLength": pos_len, "target": 34962},
        {"buffer": 0, "byteOffset": nor_off, "byteLength": nor_len, "target": 34962},
        {"buffer": 0, "byteOffset": uv_off, "byteLength": uv_len, "target": 34962},
        {"buffer": 0, "byteOffset": idx_off, "byteLength": idx_len, "target": 34963},
    ]
    accessors = [
        {"bufferView": 0, "componentType": 5126, "count": len(all_pos), "type": "VEC3", "min": pmin, "max": pmax},
        {"bufferView": 1, "componentType": 5126, "count": len(all_nor), "type": "VEC3"},
        {"bufferView": 2, "componentType": 5126, "count": len(all_uv), "type": "VEC2"},
        {"bufferView": 3, "componentType": idx_comp_type, "count": len(all_idx), "type": "SCALAR", "min": [0], "max": [max(all_idx)]},
    ]

    out_gltf = {
        "asset": {"version": "2.0", "generator": "ABFP KayKit Unified Merger"},
        "scene": 0,
        "scenes": [{"name": "Scene", "nodes": [0]}],
        "nodes": [{"name": dst_path.stem, "mesh": 0}],
        "meshes": [
            {
                "name": dst_path.stem,
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
                "name": f"{dst_path.stem}_Mat",
                "pbrMetallicRoughness": {
                    "baseColorFactor": [1.0, 1.0, 1.0, 1.0],
                    "metallicFactor": 0.0,
                    "roughnessFactor": 0.82,
                },
            }
        ],
        "bufferViews": buffer_views,
        "accessors": accessors,
        "buffers": [],
    }

    if img_bytes:
        img_off, img_len = add_chunk(img_bytes)
        buffer_views.append({"buffer": 0, "byteOffset": img_off, "byteLength": img_len})
        out_gltf["images"] = [{"bufferView": 4, "mimeType": img_mime, "name": f"{dst_path.stem}_Tex"}]
        out_gltf["textures"] = [{"source": 0}]
        out_gltf["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"] = {"index": 0, "texCoord": 0}

    while len(buf) % 4 != 0:
        buf.append(0)
    out_gltf["buffers"] = [{"byteLength": len(buf)}]

    jbytes = json.dumps(out_gltf, separators=(",", ":")).encode("utf-8")
    while len(jbytes) % 4 != 0:
        jbytes += b" "

    total_len = 12 + 8 + len(jbytes) + 8 + len(buf)
    header = struct.pack("<III", 0x46546C67, 2, total_len)
    jchunk = struct.pack("<II", len(jbytes), 0x4E4F534A) + jbytes
    bchunk = struct.pack("<II", len(buf), 0x004E4942) + bytes(buf)

    dst_path.parent.mkdir(parents=True, exist_ok=True)
    dst_path.write_bytes(header + jchunk + bchunk)
    return True


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("Usage: merge-kaykit-glb.py <src.glb> <dst.glb>")
    ok = merge_kaykit_glb(Path(sys.argv[1]), Path(sys.argv[2]))
    raise SystemExit(0 if ok else 1)
