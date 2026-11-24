"""Unreal Engine 5.x Editor Python script for Angband: First Person.

Imports all CC0 assets from assets/raw/ (fetched by tools/fetch-assets.py),
builds PBR Materials from the Poly Haven 2K texture sets (Diffuse, DirectX Normal,
and ARM Ambient Occlusion / Roughness / Metallic), and populates
/Game/Data/DA_DungeonVisualCatalog (UABFPVisualCatalog).

Run inside Unreal Editor:
  Tools -> Execute Python Script -> <repo>/tools/import-assets-to-unreal.py

Or headless via UnrealEditor-Cmd.exe:
  UnrealEditor-Cmd.exe "<project>.uproject" -run=pythonscript -script="<repo>/tools/import-assets-to-unreal.py" -stdout -FullStdOutLogOutput -nop4 -unattended -nosplash
"""

import importlib.util
import json
import os
import struct
import traceback
from pathlib import Path

import unreal


def load_kaykit_merger(repo_root: Path):
    merger_path = repo_root / "tools" / "merge-kaykit-glb.py"
    if not merger_path.is_file():
        return None
    spec = importlib.util.spec_from_file_location("abfp_merge_kaykit", str(merger_path))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod.merge_kaykit_glb


def find_repo_root() -> Path:
    env_root = os.environ.get("ABFP_REPO_ROOT")
    if env_root:
        p = Path(env_root).resolve()
        if (p / "assets" / "manifest.json").is_file():
            return p
    if "__file__" in globals():
        p = Path(__file__).resolve().parent.parent
        if (p / "assets" / "manifest.json").is_file():
            return p
    proj_dir = Path(unreal.Paths.project_dir()).resolve()
    for candidate in (proj_dir.parent, proj_dir):
        if (candidate / "assets" / "manifest.json").is_file():
            return candidate
    raise RuntimeError("Could not locate repo root containing assets/manifest.json (set ABFP_REPO_ROOT).")


def import_asset(source_file: Path, dest_path: str, asset_name: str = None, is_skeletal: bool = False) -> list:
    if not unreal.EditorAssetLibrary.does_directory_exist(dest_path):
        unreal.EditorAssetLibrary.make_directory(dest_path)

    task = unreal.AssetImportTask()
    task.filename = str(source_file)
    task.destination_path = dest_path
    if asset_name:
        task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = True

    ext = source_file.suffix.lower()
    if ext == ".fbx":
        opts = unreal.FbxImportUI()
        opts.import_mesh = True
        opts.import_textures = True
        opts.import_materials = True
        opts.import_as_skeletal = is_skeletal
        opts.import_animations = is_skeletal
        if is_skeletal:
            opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
            try:
                opts.skeletal_mesh_import_data.set_editor_property("import_meshes_in_bone_hierarchy", True)
            except Exception:
                pass
        else:
            opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
            opts.static_mesh_import_data.combine_meshes = True
        task.options = opts

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return list(task.imported_object_paths)


def find_asset_in_paths_or_folder(imported_paths: list, folder_path: str, asset_class, name_hint: str = ""):
    hint_lower = name_hint.lower()
    fallback = None

    # 1. Check directly returned imported object paths first (most reliable in headless commandlets).
    for obj_path in imported_paths or []:
        obj = unreal.EditorAssetLibrary.load_asset(obj_path)
        if isinstance(obj, asset_class):
            if not hint_lower or hint_lower in obj.get_name().lower():
                return obj
            if fallback is None:
                fallback = obj

    # 2. Fall back to listing all assets under the destination folder.
    if unreal.EditorAssetLibrary.does_directory_exist(folder_path):
        for asset_path in unreal.EditorAssetLibrary.list_assets(folder_path, recursive=True, include_folder=False):
            obj = unreal.EditorAssetLibrary.load_asset(asset_path)
            if isinstance(obj, asset_class):
                if not hint_lower or hint_lower in obj.get_name().lower():
                    return obj
                if fallback is None:
                    fallback = obj

    return fallback


def connect_uv_to_sampler(mel, uv_node, sampler_node):
    for pin_name in ("UVs", "Coordinates"):
        try:
            if mel.connect_material_expressions(uv_node, "", sampler_node, pin_name):
                return True
        except Exception:
            continue
    return False


def connect_rgb_to_property(mel, from_node, mat_prop, label=""):
    """Connect a TextureSample/TextureSampleParameter2D RGB output to a material property.
    In UE 5.x, TextureSample output 0 (RGB) is named '' (NAME_None), not 'RGB'."""
    for out_pin in ("", "RGBA", "RGB"):
        try:
            if mel.connect_material_property(from_node, out_pin, mat_prop):
                return True
        except Exception:
            continue
    unreal.log_warning(f"[ABFP] Failed connecting {label} output pin to {mat_prop}")
    return False


def build_pbr_material(mat_name: str, tex_dir: Path, unreal_tex_folder: str, unreal_mat_folder: str, uv_tiling: float = 2.0):
    """Import Poly Haven 2K Diffuse, DirectX Normal, and ARM maps and wire a UE PBR Material."""
    diff_file = next(tex_dir.glob("*_diff_*"), None)
    nor_file = next(tex_dir.glob("*_nor_dx_*"), None)
    arm_file = next(tex_dir.glob("*_arm_*"), None)

    diff_tex = None
    nor_tex = None
    arm_tex = None

    if diff_file:
        paths = import_asset(diff_file, unreal_tex_folder)
        diff_tex = find_asset_in_paths_or_folder(paths, unreal_tex_folder, unreal.Texture2D, diff_file.stem)
    if nor_file:
        paths = import_asset(nor_file, unreal_tex_folder)
        nor_tex = find_asset_in_paths_or_folder(paths, unreal_tex_folder, unreal.Texture2D, nor_file.stem)
        if isinstance(nor_tex, unreal.Texture2D):
            nor_tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            nor_tex.set_editor_property("srgb", False)
            unreal.EditorAssetLibrary.save_loaded_asset(nor_tex)
    if arm_file:
        paths = import_asset(arm_file, unreal_tex_folder)
        arm_tex = find_asset_in_paths_or_folder(paths, unreal_tex_folder, unreal.Texture2D, arm_file.stem)
        if isinstance(arm_tex, unreal.Texture2D):
            arm_tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            arm_tex.set_editor_property("srgb", False)
            unreal.EditorAssetLibrary.save_loaded_asset(arm_tex)

    if not unreal.EditorAssetLibrary.does_directory_exist(unreal_mat_folder):
        unreal.EditorAssetLibrary.make_directory(unreal_mat_folder)

    mat_asset_path = f"{unreal_mat_folder}/{mat_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(mat_asset_path):
        mat = unreal.EditorAssetLibrary.load_asset(mat_asset_path)
    else:
        factory = unreal.MaterialFactoryNew()
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            mat_name, unreal_mat_folder, unreal.Material, factory
        )

    if not isinstance(mat, unreal.Material):
        return None

    mel = unreal.MaterialEditingLibrary
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("used_with_instanced_static_meshes", True)

    uv_node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -680, 40)
    uv_node.set_editor_property("u_tiling", float(uv_tiling))
    uv_node.set_editor_property("v_tiling", float(uv_tiling))

    if diff_tex:
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -420, -240)
        node.texture = diff_tex
        connect_uv_to_sampler(mel, uv_node, node)
        connect_rgb_to_property(mel, node, unreal.MaterialProperty.MP_BASE_COLOR, f"{mat_name}.BaseColor")

    if nor_tex:
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -420, 40)
        node.texture = nor_tex
        node.sampler_type = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
        connect_uv_to_sampler(mel, uv_node, node)
        connect_rgb_to_property(mel, node, unreal.MaterialProperty.MP_NORMAL, f"{mat_name}.Normal")

    if arm_tex:
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -420, 320)
        node.texture = arm_tex
        node.sampler_type = unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
        connect_uv_to_sampler(mel, uv_node, node)
        mel.connect_material_property(node, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
        mel.connect_material_property(node, "G", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.connect_material_property(node, "B", unreal.MaterialProperty.MP_METALLIC)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(mat_asset_path, only_if_is_dirty=False)
    return mat


def extract_glb_embedded_png(glb_path: Path, out_png: Path) -> bool:
    """Extract the first embedded PNG image from a binary glTF (.glb) file."""
    try:
        data = glb_path.read_bytes()
        if len(data) < 28 or data[:4] != b"glTF":
            return False
        json_len = struct.unpack_from("<I", data, 12)[0]
        gltf = json.loads(data[20 : 20 + json_len].decode("utf-8"))
        bin_offset = 20 + json_len + 8
        images = gltf.get("images", [])
        if not images:
            return False
        bv = gltf["bufferViews"][images[0]["bufferView"]]
        start = bin_offset + bv.get("byteOffset", 0)
        img_bytes = data[start : start + bv["byteLength"]]
        if not img_bytes.startswith(b"\x89PNG\r\n\x1a\n"):
            return False
        out_png.parent.mkdir(parents=True, exist_ok=True)
        out_png.write_bytes(img_bytes)
        return True
    except Exception as exc:
        unreal.log_warning(f"[ABFP] Could not extract embedded PNG from {glb_path}: {exc}")
        return False


def build_kaykit_dungeon_material(raw_root: Path, unreal_mat_folder: str):
    """Build a single ISM-safe Material using KayKit Dungeon's embedded palette texture
    so every KayKit prop (doors, pillars, torches, banners, barrels, crates, candles)
    renders with full medieval colors and zero InstancedStaticMeshes warnings."""
    terrain_dir = raw_root / "terrain"
    palette_png = terrain_dir / "kaykit_dungeon_palette.png"
    if not palette_png.is_file() and terrain_dir.exists():
        for glb in sorted(terrain_dir.glob("*.glb")):
            if extract_glb_embedded_png(glb, palette_png):
                unreal.log(f"[ABFP] Extracted KayKit dungeon palette PNG from {glb.name}")
                break

    if not palette_png.is_file():
        return None

    tex_folder = "/Game/Assets/Materials/Textures/KayKitDungeon"
    paths = import_asset(palette_png, tex_folder, asset_name="T_KayKitDungeonPalette")
    palette_tex = find_asset_in_paths_or_folder(paths, tex_folder, unreal.Texture2D, "KayKitDungeonPalette")
    if not palette_tex:
        return None

    if not unreal.EditorAssetLibrary.does_directory_exist(unreal_mat_folder):
        unreal.EditorAssetLibrary.make_directory(unreal_mat_folder)

    mat_name = "M_ABFP_KayKitDungeon"
    mat_asset_path = f"{unreal_mat_folder}/{mat_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(mat_asset_path):
        mat = unreal.EditorAssetLibrary.load_asset(mat_asset_path)
    else:
        factory = unreal.MaterialFactoryNew()
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            mat_name, unreal_mat_folder, unreal.Material, factory
        )

    if not isinstance(mat, unreal.Material):
        return None

    mel = unreal.MaterialEditingLibrary
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("used_with_instanced_static_meshes", True)

    tex_node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -420, -140)
    tex_node.texture = palette_tex
    connect_rgb_to_property(mel, tex_node, unreal.MaterialProperty.MP_BASE_COLOR, "KayKitDungeon.BaseColor")

    rough_const = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -320, 140)
    rough_const.set_editor_property("r", 0.78)
    mel.connect_material_property(rough_const, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(mat_asset_path, only_if_is_dirty=False)
    return mat


def build_remembered_material(unreal_mat_folder: str):
    """Build a dark cool-stone ISM-safe Material for explored fog-of-war cells."""
    if not unreal.EditorAssetLibrary.does_directory_exist(unreal_mat_folder):
        unreal.EditorAssetLibrary.make_directory(unreal_mat_folder)

    mat_name = "M_ABFP_RememberedStone"
    mat_asset_path = f"{unreal_mat_folder}/{mat_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(mat_asset_path):
        mat = unreal.EditorAssetLibrary.load_asset(mat_asset_path)
    else:
        factory = unreal.MaterialFactoryNew()
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            mat_name, unreal_mat_folder, unreal.Material, factory
        )

    if not isinstance(mat, unreal.Material):
        return None

    mel = unreal.MaterialEditingLibrary
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("used_with_instanced_static_meshes", True)

    col = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -360, -100)
    col.set_editor_property("constant", unreal.LinearColor(0.14, 0.15, 0.18, 1.0))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -320, 100)
    rough.set_editor_property("r", 0.92)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(mat_asset_path, only_if_is_dirty=False)
    return mat


def enable_instanced_mesh_usage_in_folder(folder_path: str) -> int:
    """Ensure every Material (including base materials of Interchange MaterialInstances)
    under folder_path has bUsedWithInstancedStaticMeshes=True so UInstancedStaticMeshComponent
    never falls back to WorldGridMaterial."""
    if not unreal.EditorAssetLibrary.does_directory_exist(folder_path):
        return 0

    mel = unreal.MaterialEditingLibrary
    updated_paths = set()
    for asset_path in unreal.EditorAssetLibrary.list_assets(folder_path, recursive=True, include_folder=False):
        obj = unreal.EditorAssetLibrary.load_asset(asset_path)
        base_mat = None
        if isinstance(obj, unreal.Material):
            base_mat = obj
        elif isinstance(obj, unreal.MaterialInterface):
            try:
                base_mat = obj.get_base_material()
            except Exception:
                base_mat = None
        if not isinstance(base_mat, unreal.Material):
            continue
        base_path = base_mat.get_path_name().split(".")[0]
        if base_path in updated_paths or not base_path.startswith("/Game/"):
            continue
        try:
            if not base_mat.get_editor_property("used_with_instanced_static_meshes"):
                base_mat.set_editor_property("used_with_instanced_static_meshes", True)
                mel.recompile_material(base_mat)
                unreal.EditorAssetLibrary.save_loaded_asset(base_mat)
                updated_paths.add(base_path)
        except Exception as exc:
            unreal.log_warning(f"[ABFP] Could not enable InstancedStaticMeshes on {base_path}: {exc}")

    # Also resave any MaterialInstances under the folder so they don't recompile every editor launch.
    for asset_path in unreal.EditorAssetLibrary.list_assets(folder_path, recursive=True, include_folder=False):
        obj = unreal.EditorAssetLibrary.load_asset(asset_path)
        if isinstance(obj, unreal.MaterialInstanceConstant):
            try:
                unreal.EditorAssetLibrary.save_loaded_asset(obj)
            except Exception:
                pass
    return len(updated_paths)


def build_instanced_master_material(unreal_mat_folder: str):
    """Build a lightweight ISM-safe master Material for glTF/KayKit terrain meshes so they
    never reference Interchange base materials that lack bUsedWithInstancedStaticMeshes."""
    if not unreal.EditorAssetLibrary.does_directory_exist(unreal_mat_folder):
        unreal.EditorAssetLibrary.make_directory(unreal_mat_folder)

    mat_name = "M_ABFP_InstancedMaster"
    mat_asset_path = f"{unreal_mat_folder}/{mat_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(mat_asset_path):
        mat = unreal.EditorAssetLibrary.load_asset(mat_asset_path)
    else:
        factory = unreal.MaterialFactoryNew()
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            mat_name, unreal_mat_folder, unreal.Material, factory
        )

    if not isinstance(mat, unreal.Material):
        return None

    mel = unreal.MaterialEditingLibrary
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("used_with_instanced_static_meshes", True)

    tex_param = mel.create_material_expression(
        mat, unreal.MaterialExpressionTextureSampleParameter2D, -420, -140
    )
    tex_param.set_editor_property("parameter_name", "BaseColorTexture")
    connect_rgb_to_property(mel, tex_param, unreal.MaterialProperty.MP_BASE_COLOR, "InstancedMaster.BaseColor")

    rough_const = mel.create_material_expression(
        mat, unreal.MaterialExpressionConstant, -320, 140
    )
    rough_const.set_editor_property("r", 0.78)
    mel.connect_material_property(rough_const, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(mat_asset_path, only_if_is_dirty=False)
    return mat


def make_instanced_material_for_mesh(entry_id: str, imported_paths: list, dest_sub: str, master_mat):
    """Create a saved MaterialInstanceConstant derived from M_ABFP_InstancedMaster using the
    mesh's own imported BaseColor Texture2D."""
    if not master_mat:
        return None
    inst_folder = "/Game/Assets/Materials/Instances"
    if not unreal.EditorAssetLibrary.does_directory_exist(inst_folder):
        unreal.EditorAssetLibrary.make_directory(inst_folder)

    inst_name = f"MI_{entry_id}"
    inst_path = f"{inst_folder}/{inst_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(inst_path):
        mic = unreal.EditorAssetLibrary.load_asset(inst_path)
    else:
        factory = unreal.MaterialInstanceConstantFactoryNew()
        mic = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            inst_name, inst_folder, unreal.MaterialInstanceConstant, factory
        )

    if not isinstance(mic, unreal.MaterialInstanceConstant):
        return None

    mel = unreal.MaterialEditingLibrary
    mel.set_material_instance_parent(mic, master_mat)
    tex = find_asset_in_paths_or_folder(imported_paths, dest_sub, unreal.Texture2D)
    if tex:
        mel.set_material_instance_texture_parameter_value(mic, "BaseColorTexture", tex)
    unreal.EditorAssetLibrary.save_asset(inst_path, only_if_is_dirty=False)
    return mic


def assign_material_all_slots(mesh, material):
    if not mesh or not material:
        return
    try:
        slots = mesh.get_editor_property("static_materials")
        count = max(1, len(slots) if slots is not None else 1)
    except Exception:
        count = 1
    for slot_idx in range(count):
        mesh.set_material(slot_idx, material)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)


def build_torch_flame_material(unreal_mat_folder: str):
    """Build a glowing emissive ISM-safe Material for wall torch flames (DX11/SM5 bloom friendly)."""
    if not unreal.EditorAssetLibrary.does_directory_exist(unreal_mat_folder):
        unreal.EditorAssetLibrary.make_directory(unreal_mat_folder)

    mat_name = "M_ABFP_TorchFlame"
    mat_asset_path = f"{unreal_mat_folder}/{mat_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(mat_asset_path):
        mat = unreal.EditorAssetLibrary.load_asset(mat_asset_path)
    else:
        factory = unreal.MaterialFactoryNew()
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            mat_name, unreal_mat_folder, unreal.Material, factory
        )

    if not isinstance(mat, unreal.Material):
        return None

    mel = unreal.MaterialEditingLibrary
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("used_with_instanced_static_meshes", True)

    base_col = mel.create_material_expression(
        mat, unreal.MaterialExpressionConstant3Vector, -360, -140
    )
    base_col.set_editor_property("constant", unreal.LinearColor(1.0, 0.58, 0.12, 1.0))
    mel.connect_material_property(base_col, "", unreal.MaterialProperty.MP_BASE_COLOR)

    emissive_col = mel.create_material_expression(
        mat, unreal.MaterialExpressionConstant3Vector, -360, 80
    )
    emissive_col.set_editor_property("constant", unreal.LinearColor(8.5, 3.4, 0.55, 1.0))
    mel.connect_material_property(emissive_col, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(mat_asset_path, only_if_is_dirty=False)
    return mat


def build_creature_textured_material(entry_id: str, tex_path: Path, roughness: float = 0.80):
    """Build a skeletal-mesh PBR Material directly from a companion character texture PNG
    (used for KayKit FBX characters so every outfit/hood/armor renders crisply)."""
    if not tex_path.is_file():
        return None
    tex_folder = "/Game/Assets/Materials/Textures/Creatures"
    tex_name = f"T_Char_{tex_path.stem}"
    paths = import_asset(tex_path, tex_folder, asset_name=tex_name)
    tex = find_asset_in_paths_or_folder(paths, tex_folder, unreal.Texture2D, tex_name)
    if not tex:
        return None

    mat_folder = "/Game/Assets/Materials/Creatures"
    if not unreal.EditorAssetLibrary.does_directory_exist(mat_folder):
        unreal.EditorAssetLibrary.make_directory(mat_folder)

    mat_name = f"M_ABFP_CharTex_{tex_path.stem}"
    mat_asset_path = f"{mat_folder}/{mat_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(mat_asset_path):
        mat = unreal.EditorAssetLibrary.load_asset(mat_asset_path)
    else:
        factory = unreal.MaterialFactoryNew()
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            mat_name, mat_folder, unreal.Material, factory
        )
    if not isinstance(mat, unreal.Material):
        return None

    mel = unreal.MaterialEditingLibrary
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("used_with_skeletal_mesh", True)

    node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -420, -140)
    node.texture = tex
    connect_rgb_to_property(mel, node, unreal.MaterialProperty.MP_BASE_COLOR, f"{mat_name}.BaseColor")

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -320, 100)
    rough.set_editor_property("r", float(roughness))
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(mat_asset_path, only_if_is_dirty=False)
    return mat


def build_creature_color_material(entry_id: str, color_rgb, use_vertex_color: bool = False, roughness: float = 0.72):
    """Build a rich PBR Material for untextured/vertex-colored CC0 creature meshes so they never
    appear as flat untextured white clay."""
    mat_folder = "/Game/Assets/Materials/Creatures"
    if not unreal.EditorAssetLibrary.does_directory_exist(mat_folder):
        unreal.EditorAssetLibrary.make_directory(mat_folder)

    mat_name = f"M_ABFP_Creature_{entry_id}"
    mat_asset_path = f"{mat_folder}/{mat_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(mat_asset_path):
        mat = unreal.EditorAssetLibrary.load_asset(mat_asset_path)
    else:
        factory = unreal.MaterialFactoryNew()
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            mat_name, mat_folder, unreal.Material, factory
        )

    if not isinstance(mat, unreal.Material):
        return None

    mel = unreal.MaterialEditingLibrary
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("used_with_skeletal_mesh", True)

    if use_vertex_color:
        vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -420, -140)
        connect_rgb_to_property(mel, vc, unreal.MaterialProperty.MP_BASE_COLOR, f"{mat_name}.VertexColor")
    else:
        col = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -360, -140)
        col.set_editor_property(
            "constant",
            unreal.LinearColor(float(color_rgb[0]), float(color_rgb[1]), float(color_rgb[2]), 1.0),
        )
        mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)

    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -320, 100)
    rough.set_editor_property("r", float(roughness))
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(mat_asset_path, only_if_is_dirty=False)
    return mat


def compute_creature_fit(mesh, entry: dict):
    """Compute uniform scale and floor-grounding Z offset from the imported Unreal mesh bounds
    so every creature matches its target_height_cm and stands cleanly on the floor."""
    fallback_scale = tuple(entry.get("scale", [1.0, 1.0, 1.0]))
    base_loc = list(entry.get("location_offset", [0.0, 0.0, 0.0]))
    target_h = float(entry.get("target_height_cm", 0.0))
    if not mesh or target_h <= 0.0:
        return fallback_scale, tuple(base_loc)
    try:
        bounds = mesh.get_bounds()
        ext = bounds.box_extent
        orig = bounds.origin
        raw_h = max(1.0, float(ext.z) * 2.0)
        raw_xy = max(1.0, float(ext.x) * 2.0, float(ext.y) * 2.0)
        fit_scale = min(target_h / raw_h, 310.0 / raw_xy)
        fit_scale = max(0.01, min(fit_scale, 30.0))
        bottom_z = float(orig.z) - float(ext.z)
        # Only lift upward if the mesh actually extends below Z=0 (centered pivot);
        # never push already-grounded skeletal meshes (e.g. KayKit) underground!
        if bottom_z < -5.0:
            base_loc[2] = float(base_loc[2]) - bottom_z * fit_scale
        unreal.log(
            f"[ABFP] Fitted {entry['id']}: raw=({ext.x*2:.0f}x{ext.y*2:.0f}x{raw_h:.0f}cm) "
            f"-> scale={fit_scale:.3f}, z_off={base_loc[2]:.1f}cm (target_h={target_h:.0f}cm)"
        )
        return (fit_scale, fit_scale, fit_scale), tuple(base_loc)
    except Exception as exc:
        unreal.log_warning(f"[ABFP] Could not compute bounds for {entry['id']}: {exc}")
        return fallback_scale, tuple(base_loc)


def make_mesh_variant(mesh, material=None, scale=(1.0, 1.0, 1.0), loc=(0.0, 0.0, 0.0), rot=(0.0, 0.0, 0.0)):
    v = unreal.ABFPMeshVariant()
    v.set_editor_property("mesh", mesh)
    if material:
        v.set_editor_property("material_override", material)
    v.set_editor_property("location_offset", unreal.Vector(*loc))
    v.set_editor_property("rotation_offset", unreal.Rotator(rot[0], rot[1], rot[2]))
    v.set_editor_property("scale", unreal.Vector(*scale))
    return v


def main():
    unreal.log("[ABFP] Starting Unreal asset import...")
    repo_root = find_repo_root()
    manifest_path = repo_root / "assets" / "manifest.json"
    raw_root = repo_root / "assets" / "raw"

    if not raw_root.exists():
        raise RuntimeError(f"assets/raw/ not found at {raw_root}. Run tools/fetch-assets.py first.")

    if not hasattr(unreal, "ABFPVisualCatalog"):
        raise RuntimeError(
            "unreal.ABFPVisualCatalog class not found. Compile the AngbandFirstPerson C++ module first."
        )

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))

    # 1. Build Poly Haven 2K PBR Materials + ISM-safe KayKit, Remembered & Flame materials
    instanced_master_mat = build_instanced_master_material("/Game/Assets/Materials")
    kaykit_dungeon_mat = build_kaykit_dungeon_material(raw_root, "/Game/Assets/Materials")
    remembered_mat = build_remembered_material("/Game/Assets/Materials")
    torch_flame_mat = build_torch_flame_material("/Game/Assets/Materials")
    materials_by_name = {}
    for entry in manifest.get("pbr_materials", []):
        tex_dir = raw_root / entry["dest_dir"]
        if not tex_dir.exists():
            continue
        mat_name = entry["unreal_material_name"]
        try:
            unreal.log(f"[ABFP] Building PBR material {mat_name}...")
            mat = build_pbr_material(
                mat_name,
                tex_dir,
                f"/Game/Assets/Materials/Textures/{entry['id']}",
                "/Game/Assets/Materials",
                uv_tiling=float(entry.get("uv_tiling", 1.5)),
            )
            if mat:
                materials_by_name[mat_name] = mat
        except Exception as exc:
            unreal.log_warning(f"[ABFP] Failed building material {mat_name}: {exc}")

    # 2. Import Audio Cues
    audio_by_id = {}
    for entry in manifest.get("audio", []):
        src = raw_root / entry["dest"]
        if not src.exists():
            continue
        try:
            paths = import_asset(src, entry["unreal_dest"], asset_name=entry["id"])
            sound = find_asset_in_paths_or_folder(paths, entry["unreal_dest"], unreal.SoundBase, entry["id"])
            if sound:
                audio_by_id[entry["id"]] = sound
        except Exception as exc:
            unreal.log_warning(f"[ABFP] Failed importing audio {entry['id']}: {exc}")

    # 3. Create or load DA_DungeonVisualCatalog
    catalog_folder = "/Game/Data"
    catalog_name = "DA_DungeonVisualCatalog"
    catalog_path = f"{catalog_folder}/{catalog_name}"
    if not unreal.EditorAssetLibrary.does_directory_exist(catalog_folder):
        unreal.EditorAssetLibrary.make_directory(catalog_folder)

    if unreal.EditorAssetLibrary.does_asset_exist(catalog_path):
        catalog = unreal.EditorAssetLibrary.load_asset(catalog_path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.ABFPVisualCatalog.static_class())
        catalog = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            catalog_name, catalog_folder, unreal.ABFPVisualCatalog, factory
        )

    if not catalog:
        raise RuntimeError(f"Failed to create or load {catalog_path}.")

    dungeon_terrain = catalog.get_editor_property("dungeon_terrain")
    town_terrain = catalog.get_editor_property("town_terrain")

    floor_variants = []
    town_floor_variants = []
    wall_variants = []
    vein_variants = []
    pillar_variants = []
    wall_decor_variants = []
    ceiling_variants = []

    # 4a. Build watertight solid 400x400x320cm Walls/Veins and 400x400x12cm Floors/Ceilings
    # using UE's box-UV BasicShapes/Cube paired with our 2K Poly Haven PBR stone/cobble materials.
    # This eliminates open-slab alcoves, corner gaps, and KayKit collapsed palette UVs.
    cube_mesh = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube")
    if cube_mesh:
        for mat_name in ("M_DungeonFloor_Slate", "M_DungeonFloor_Worn"):
            mat = materials_by_name.get(mat_name)
            if mat:
                floor_variants.append(
                    make_mesh_variant(cube_mesh, material=mat, scale=(4.0, 4.0, 0.12), loc=(0.0, 0.0, -6.0))
                )
        town_cobble = materials_by_name.get("M_TownFloor_Cobble") or materials_by_name.get("M_DungeonFloor_Slate")
        if town_cobble:
            town_floor_variants.append(
                make_mesh_variant(cube_mesh, material=town_cobble, scale=(4.0, 4.0, 0.12), loc=(0.0, 0.0, -6.0))
            )
        ceil_mat = materials_by_name.get("M_DungeonCeiling_Stone") or materials_by_name.get("M_DungeonWall_Ashlar")
        if ceil_mat:
            ceiling_variants.append(
                make_mesh_variant(cube_mesh, material=ceil_mat, scale=(4.0, 4.0, 0.12), loc=(0.0, 0.0, 6.0))
            )
        for mat_name in ("M_DungeonWall_Ashlar", "M_DungeonWall_Brick"):
            mat = materials_by_name.get(mat_name)
            if mat:
                wall_variants.append(
                    make_mesh_variant(cube_mesh, material=mat, scale=(4.0, 4.0, 3.2), loc=(0.0, 0.0, 160.0))
                )
        vein_mat = materials_by_name.get("M_DungeonVein_Rock")
        if vein_mat:
            vein_variants.append(
                make_mesh_variant(cube_mesh, material=vein_mat, scale=(4.0, 4.0, 3.2), loc=(0.0, 0.0, 160.0))
            )

    # 4b. Import Modular Dungeon Props (Pillars, Doors, Stairs, Traps, Torches, Shop Archways, Wall Decor)
    for entry in manifest.get("terrain_meshes", []):
        src = raw_root / entry["dest"]
        if not src.exists():
            continue
        try:
            unreal.log(f"[ABFP] Importing terrain prop {entry['id']}...")
            dest_sub = f"{entry['unreal_dest']}/{entry['id']}"
            paths = import_asset(src, dest_sub, is_skeletal=False)
            mesh = find_asset_in_paths_or_folder(paths, dest_sub, unreal.StaticMesh)
            if not mesh:
                unreal.log_warning(f"[ABFP] No StaticMesh imported for {entry['id']}")
                continue

            mat_override = materials_by_name.get(entry.get("material_override", ""))
            if not mat_override:
                mat_override = kaykit_dungeon_mat or make_instanced_material_for_mesh(
                    entry["id"], paths, dest_sub, instanced_master_mat
                )
            assign_material_all_slots(mesh, mat_override)
            variant = make_mesh_variant(
                mesh,
                material=mat_override,
                scale=tuple(entry.get("scale", [1.0, 1.0, 1.0])),
                loc=tuple(entry.get("location_offset", [0.0, 0.0, 0.0])),
                rot=tuple(entry.get("rotation_offset", [0.0, 0.0, 0.0])),
            )
            slot = entry.get("catalog_slot", "")
            if slot == "dungeon.floor":
                floor_variants.append(variant)
            elif slot == "town.floor":
                town_floor_variants.append(variant)
            elif slot == "dungeon.wall":
                wall_variants.append(variant)
            elif slot == "dungeon.vein":
                vein_variants.append(variant)
            elif slot == "dungeon.pillar":
                pillar_variants.append(variant)
            elif slot == "dungeon.wall_decor":
                wall_decor_variants.append(variant)
            elif slot == "dungeon.door_frame":
                dungeon_terrain.set_editor_property("door_frame", variant)
            elif slot == "dungeon.door_closed":
                dungeon_terrain.set_editor_property("door_closed_panel", variant)
            elif slot == "dungeon.door_open":
                dungeon_terrain.set_editor_property("door_open_panel", variant)
            elif slot == "dungeon.stairs_up":
                dungeon_terrain.set_editor_property("stairs_up", variant)
            elif slot == "dungeon.stairs_down":
                dungeon_terrain.set_editor_property("stairs_down", variant)
            elif slot == "dungeon.rubble":
                dungeon_terrain.set_editor_property("rubble", variant)
            elif slot == "dungeon.trap":
                dungeon_terrain.set_editor_property("trap_marker", variant)
            elif slot == "dungeon.shop":
                dungeon_terrain.set_editor_property("shop_facade", variant)
                town_terrain.set_editor_property("shop_facade", variant)
            elif slot == "dungeon.wall_torch":
                dungeon_terrain.set_editor_property("wall_torch_prop", variant)
        except Exception as exc:
            unreal.log_warning(f"[ABFP] Failed importing terrain {entry['id']}: {exc}")

    dungeon_terrain.set_editor_property("floor_variants", floor_variants)
    dungeon_terrain.set_editor_property("ceiling_variants", ceiling_variants)
    dungeon_terrain.set_editor_property("wall_variants", wall_variants)
    dungeon_terrain.set_editor_property("vein_wall_variants", vein_variants)
    dungeon_terrain.set_editor_property("wall_pillar_variants", [])
    dungeon_terrain.set_editor_property("wall_decor_variants", wall_decor_variants)
    town_terrain.set_editor_property("floor_variants", town_floor_variants or floor_variants)
    town_terrain.set_editor_property("wall_variants", wall_variants)
    town_terrain.set_editor_property("wall_pillar_variants", [])
    town_terrain.set_editor_property("wall_decor_variants", wall_decor_variants)
    if remembered_mat:
        dungeon_terrain.set_editor_property("remembered_material_override", remembered_mat)
        town_terrain.set_editor_property("remembered_material_override", remembered_mat)

    # Photorealistic 2K stone door frames & dark-oak timber doors / town shop portals
    # (Mesh=None lets ABFPDungeonActor build watertight 400x320cm frames/doors matching our 2K walls)
    stone_frame_mat = materials_by_name.get("M_DungeonWall_Ashlar")
    wood_mat = materials_by_name.get("M_DungeonDoor_Wood")
    if stone_frame_mat:
        frame_v = unreal.ABFPMeshVariant()
        frame_v.set_editor_property("material_override", stone_frame_mat)
        dungeon_terrain.set_editor_property("door_frame", frame_v)
        town_terrain.set_editor_property("door_frame", frame_v)
    if wood_mat:
        wood_v = unreal.ABFPMeshVariant()
        wood_v.set_editor_property("material_override", wood_mat)
        dungeon_terrain.set_editor_property("door_closed_panel", wood_v)
        dungeon_terrain.set_editor_property("door_open_panel", wood_v)
        dungeon_terrain.set_editor_property("door_broken_panel", wood_v)
        dungeon_terrain.set_editor_property("shop_facade", wood_v)
        town_terrain.set_editor_property("shop_facade", wood_v)

    catalog.set_editor_property("dungeon_terrain", dungeon_terrain)
    catalog.set_editor_property("town_terrain", town_terrain)
    if torch_flame_mat:
        catalog.set_editor_property("torch_flame_material", torch_flame_mat)

    # 5. Import Poly Haven Hero Props (Lantern, Sword, Boulder, Goblets)
    item_entries = []
    for entry in manifest.get("polyhaven_models", []):
        model_dir = raw_root / entry["dest_dir"]
        gltf_file = next(model_dir.glob("*.gltf"), None)
        if not gltf_file:
            continue
        try:
            unreal.log(f"[ABFP] Importing Poly Haven 3D prop {entry['id']}...")
            paths = import_asset(gltf_file, entry["unreal_dest"], is_skeletal=False)
            hero_mesh = find_asset_in_paths_or_folder(paths, entry["unreal_dest"], unreal.StaticMesh)
            if entry["id"] == "Lantern_01" and hero_mesh:
                catalog.set_editor_property(
                    "first_person_lantern",
                    make_mesh_variant(hero_mesh, scale=(0.95, 0.95, 0.95), loc=(0.0, 0.0, 0.0), rot=(0.0, 180.0, 0.0)),
                )
            elif entry["id"] == "antique_estoc" and hero_mesh:
                catalog.set_editor_property(
                    "first_person_weapon",
                    make_mesh_variant(hero_mesh, scale=(0.65, 0.65, 0.65), loc=(0.0, 0.0, -6.0), rot=(38.0, -25.0, 12.0)),
                )
            if "catalog_item" in entry:
                ci = entry["catalog_item"]
                mesh = hero_mesh
                if mesh:
                    item = unreal.ABFPItemVisualEntry()
                    item.set_editor_property("entry_name", ci["entry_name"])
                    item.set_editor_property("name_contains", ci["name_contains"])
                    item.set_editor_property("static_mesh", mesh)
                    item.set_editor_property("scale", unreal.Vector(*ci.get("scale", [1, 1, 1])))
                    item.set_editor_property("location_offset", unreal.Vector(*ci.get("location_offset", [0, 0, 8])))
                    rot = ci.get("rotation_offset", [0, 0, 0])
                    item.set_editor_property("rotation_offset", unreal.Rotator(rot[0], rot[1], rot[2]))
                    item.set_editor_property("float_and_spin", bool(ci.get("float_and_spin", False)))
                    item.set_editor_property("emit_subtle_light", bool(ci.get("emit_light", False)))
                    if "light_color" in ci:
                        lc = ci["light_color"]
                        item.set_editor_property("accent_light_color", unreal.LinearColor(lc[0], lc[1], lc[2], 1.0))
                    item_entries.append(item)
        except Exception as exc:
            unreal.log_warning(f"[ABFP] Failed importing Poly Haven prop {entry['id']}: {exc}")

    # 6. Import Floor Item Meshes
    for entry in manifest.get("item_meshes", []):
        src = raw_root / entry["dest"]
        if not src.exists():
            continue
        try:
            unreal.log(f"[ABFP] Importing item mesh {entry['id']}...")
            dest_sub = f"{entry['unreal_dest']}/{entry['id']}"
            paths = import_asset(src, dest_sub, is_skeletal=False)
            mesh = find_asset_in_paths_or_folder(paths, dest_sub, unreal.StaticMesh)
            if not mesh:
                continue
            item = unreal.ABFPItemVisualEntry()
            item.set_editor_property("entry_name", entry["entry_name"])
            item.set_editor_property("name_contains", entry["name_contains"])
            item.set_editor_property("static_mesh", mesh)
            item.set_editor_property("scale", unreal.Vector(*entry.get("scale", [1, 1, 1])))
            item.set_editor_property("location_offset", unreal.Vector(*entry.get("location_offset", [0, 0, 8])))
            item.set_editor_property("float_and_spin", bool(entry.get("float_and_spin", False)))
            item.set_editor_property("emit_subtle_light", bool(entry.get("emit_light", False)))
            if "light_color" in entry:
                lc = entry["light_color"]
                item.set_editor_property("accent_light_color", unreal.LinearColor(lc[0], lc[1], lc[2], 1.0))
            item_entries.append(item)
        except Exception as exc:
            unreal.log_warning(f"[ABFP] Failed importing item {entry['id']}: {exc}")

    catalog.set_editor_property("items", item_entries)

    # 7. Import Creatures (Procedural Angband GLBs + Unified KayKit GLBs + Skeletal/Static + Audio)
    gen_path = repo_root / "tools" / "generate-angband-creatures.py"
    if gen_path.is_file():
        try:
            spec = importlib.util.spec_from_file_location("abfp_gen_creatures", str(gen_path))
            gen_mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(gen_mod)
            n_gen = gen_mod.generate_all(raw_root)
            unreal.log(f"[ABFP] Generated {n_gen} procedural Angband dungeon creature GLBs.")
        except Exception as exc:
            unreal.log_warning(f"[ABFP] Failed generating procedural creatures: {exc}")

    merge_kaykit_fn = load_kaykit_merger(repo_root)
    creature_entries = []
    default_creature_entry = None
    for entry in manifest.get("creatures", []):
        src = raw_root / entry["dest"]
        if not src.exists():
            continue
        try:
            import_src = src
            is_skeletal = bool(entry.get("rigged", False))
            if entry.get("merge_kaykit_body") and merge_kaykit_fn is not None:
                merged_path = src.with_name(f"{src.stem}_unified.glb")
                if merge_kaykit_fn(src, merged_path):
                    import_src = merged_path
                    is_skeletal = False
                    unreal.log(f"[ABFP] Merged KayKit body parts -> {merged_path.name}")
            unreal.log(f"[ABFP] Importing creature {entry['id']} from {import_src.name}...")
            # Clean stale split-part assets when switching a creature folder
            if entry.get("clean_dest_before_import") and unreal.EditorAssetLibrary.does_directory_exist(entry["unreal_dest"]):
                for old_path in unreal.EditorAssetLibrary.list_assets(entry["unreal_dest"], recursive=True, include_folder=False):
                    unreal.EditorAssetLibrary.delete_asset(old_path)
            paths = import_asset(import_src, entry["unreal_dest"], is_skeletal=is_skeletal)

            skel_mesh = find_asset_in_paths_or_folder(paths, entry["unreal_dest"], unreal.SkeletalMesh, import_src.stem) if is_skeletal else None
            static_mesh = None if skel_mesh else find_asset_in_paths_or_folder(paths, entry["unreal_dest"], unreal.StaticMesh, import_src.stem)
            if not skel_mesh and not static_mesh:
                unreal.log_warning(f"[ABFP] No mesh found after importing {entry['id']}")
                continue

            idle_anim = find_asset_in_paths_or_folder(paths, entry["unreal_dest"], unreal.AnimSequence, "idle") if skel_mesh else None
            hit_sound = audio_by_id.get(entry.get("hit_sound", ""))
            death_sound = audio_by_id.get(entry.get("death_sound", ""))
            fit_scale, fit_loc = compute_creature_fit(skel_mesh or static_mesh, entry)

            creature_mat = None
            if entry.get("character_texture") and not entry.get("merge_kaykit_body"):
                creature_mat = build_creature_textured_material(
                    entry["id"],
                    raw_root / entry["character_texture"],
                    roughness=float(entry.get("fallback_roughness", 0.80)),
                )
            elif entry.get("fallback_color") or entry.get("use_vertex_color"):
                creature_mat = build_creature_color_material(
                    entry["id"],
                    entry.get("fallback_color", [0.12, 0.08, 0.05]),
                    use_vertex_color=bool(entry.get("use_vertex_color", False)),
                    roughness=float(entry.get("fallback_roughness", 0.88)),
                )

            if entry["id"] == "kaykit_rogue_hooded" or (default_creature_entry is None and (skel_mesh is not None or static_mesh is not None)):
                dc = unreal.ABFPCreatureVisualEntry()
                dc.set_editor_property("entry_name", f"Default_{entry['id']}")
                if skel_mesh:
                    dc.set_editor_property("skeletal_mesh", skel_mesh)
                    if idle_anim:
                        dc.set_editor_property("idle_anim", idle_anim)
                elif static_mesh:
                    dc.set_editor_property("static_mesh_fallback", static_mesh)
                if creature_mat:
                    dc.set_editor_property("material_override", creature_mat)
                dc.set_editor_property("scale", unreal.Vector(*fit_scale))
                dc.set_editor_property("location_offset", unreal.Vector(*fit_loc))
                if hit_sound:
                    dc.set_editor_property("hit_sound", hit_sound)
                if death_sound:
                    dc.set_editor_property("death_sound", death_sound)
                default_creature_entry = dc

            for keyword in entry.get("keywords", []):
                ce = unreal.ABFPCreatureVisualEntry()
                ce.set_editor_property("entry_name", f"{entry['id']}_{keyword}")
                ce.set_editor_property("name_contains", keyword)
                if skel_mesh:
                    ce.set_editor_property("skeletal_mesh", skel_mesh)
                    if idle_anim:
                        ce.set_editor_property("idle_anim", idle_anim)
                elif static_mesh:
                    ce.set_editor_property("static_mesh_fallback", static_mesh)
                if creature_mat:
                    ce.set_editor_property("material_override", creature_mat)
                ce.set_editor_property("scale", unreal.Vector(*fit_scale))
                ce.set_editor_property("location_offset", unreal.Vector(*fit_loc))
                if hit_sound:
                    ce.set_editor_property("hit_sound", hit_sound)
                if death_sound:
                    ce.set_editor_property("death_sound", death_sound)
                creature_entries.append(ce)
        except Exception as exc:
            unreal.log_warning(f"[ABFP] Failed importing creature {entry['id']}: {exc}")

    catalog.set_editor_property("creatures", creature_entries)
    if default_creature_entry:
        catalog.set_editor_property("default_creature", default_creature_entry)

    # 8. Set DX11/SM5 non-inverse-square torchlight intensity defaults on the catalog
    catalog.set_editor_property("room_torch_intensity", 5.5)
    catalog.set_editor_property("room_torch_radius", 1850.0)

    # 9. Ensure all terrain and PBR materials have bUsedWithInstancedStaticMeshes=True
    ism_fixed = enable_instanced_mesh_usage_in_folder("/Game/Assets/Terrain")
    ism_fixed += enable_instanced_mesh_usage_in_folder("/Game/Assets/Materials")
    unreal.log(f"[ABFP] Enabled InstancedStaticMeshes usage flag on {ism_fixed} materials.")

    # 9. Wire Global Audio Cues
    if "footstep_a" in audio_by_id:
        catalog.set_editor_property("footstep_sound", audio_by_id["footstep_a"])
    if "hit" in audio_by_id:
        catalog.set_editor_property("attack_swing_sound", audio_by_id["hit"])
    if "grunt_hurt" in audio_by_id:
        catalog.set_editor_property("player_hit_sound", audio_by_id["grunt_hurt"])

    saved = unreal.EditorAssetLibrary.save_asset(catalog_path, only_if_is_dirty=False)
    unreal.log(
        f"[ABFP] Saved {catalog_path} (ok={saved}): "
        f"{len(materials_by_name)} PBR materials, {len(floor_variants)} floors, {len(wall_variants)} walls, "
        f"{len(creature_entries)} creature rules, {len(item_entries)} item rules, {len(audio_by_id)} sounds."
    )


if __name__ == "__main__":
    try:
        main()
    except Exception:
        unreal.log_error("[ABFP] Fatal error during asset import:\n" + traceback.format_exc())
        raise
