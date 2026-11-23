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

import json
import os
import traceback
from pathlib import Path

import unreal


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


def build_pbr_material(mat_name: str, tex_dir: Path, unreal_tex_folder: str, unreal_mat_folder: str):
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
            nor_tex.compression_settings = unreal.TextureCompressionSettings.TC_NORMALMAP
            nor_tex.srgb = False
            nor_tex.post_edit_change()
            unreal.EditorAssetLibrary.save_loaded_asset(nor_tex)
    if arm_file:
        paths = import_asset(arm_file, unreal_tex_folder)
        arm_tex = find_asset_in_paths_or_folder(paths, unreal_tex_folder, unreal.Texture2D, arm_file.stem)
        if isinstance(arm_tex, unreal.Texture2D):
            arm_tex.compression_settings = unreal.TextureCompressionSettings.TC_MASKS
            arm_tex.srgb = False
            arm_tex.post_edit_change()
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

    if diff_tex:
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -420, -240)
        node.texture = diff_tex
        mel.connect_material_property(node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

    if nor_tex:
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -420, 40)
        node.texture = nor_tex
        node.sampler_type = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
        mel.connect_material_property(node, "RGB", unreal.MaterialProperty.MP_NORMAL)

    if arm_tex:
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -420, 320)
        node.texture = arm_tex
        node.sampler_type = unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
        mel.connect_material_property(node, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
        mel.connect_material_property(node, "G", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.connect_material_property(node, "B", unreal.MaterialProperty.MP_METALLIC)

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
    mel.connect_material_property(tex_param, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)

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

    # 1. Build Poly Haven 2K PBR Materials + ISM-safe master & flame materials
    instanced_master_mat = build_instanced_master_material("/Game/Assets/Materials")
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

    # 4. Import Modular Dungeon Terrain Meshes
    for entry in manifest.get("terrain_meshes", []):
        src = raw_root / entry["dest"]
        if not src.exists():
            continue
        try:
            unreal.log(f"[ABFP] Importing terrain mesh {entry['id']}...")
            dest_sub = f"{entry['unreal_dest']}/{entry['id']}"
            paths = import_asset(src, dest_sub, is_skeletal=False)
            mesh = find_asset_in_paths_or_folder(paths, dest_sub, unreal.StaticMesh)
            if not mesh:
                unreal.log_warning(f"[ABFP] No StaticMesh imported for {entry['id']}")
                continue

            mat_override = materials_by_name.get(entry.get("material_override", ""))
            if not mat_override:
                mat_override = make_instanced_material_for_mesh(
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

    ceiling_variants = []
    if floor_variants:
        ceil_mat = materials_by_name.get("M_DungeonCeiling_Stone")
        ceiling_variants.append(
            make_mesh_variant(
                floor_variants[0].get_editor_property("mesh"),
                material=ceil_mat,
                scale=(1.0, 1.0, 1.0),
                rot=(180.0, 0.0, 0.0),
            )
        )

    dungeon_terrain.set_editor_property("floor_variants", floor_variants)
    dungeon_terrain.set_editor_property("ceiling_variants", ceiling_variants)
    dungeon_terrain.set_editor_property("wall_variants", wall_variants)
    dungeon_terrain.set_editor_property("vein_wall_variants", vein_variants)
    dungeon_terrain.set_editor_property("wall_pillar_variants", pillar_variants)
    dungeon_terrain.set_editor_property("wall_decor_variants", wall_decor_variants)
    town_terrain.set_editor_property("floor_variants", town_floor_variants or floor_variants)
    town_terrain.set_editor_property("wall_variants", wall_variants)
    town_terrain.set_editor_property("wall_decor_variants", wall_decor_variants)

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
                    make_mesh_variant(hero_mesh, scale=(0.40, 0.40, 0.40), loc=(0.0, 0.0, 0.0), rot=(0.0, 180.0, 0.0)),
                )
            elif entry["id"] == "antique_estoc" and hero_mesh:
                catalog.set_editor_property(
                    "first_person_weapon",
                    make_mesh_variant(hero_mesh, scale=(0.46, 0.46, 0.46), loc=(0.0, 0.0, 0.0), rot=(62.0, -18.0, 12.0)),
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

    # 7. Import Creatures (Skeletal + Static Fallbacks + Idle Animations + Audio)
    creature_entries = []
    for entry in manifest.get("creatures", []):
        src = raw_root / entry["dest"]
        if not src.exists():
            continue
        try:
            unreal.log(f"[ABFP] Importing creature {entry['id']}...")
            paths = import_asset(src, entry["unreal_dest"], is_skeletal=bool(entry.get("rigged", False)))

            skel_mesh = find_asset_in_paths_or_folder(paths, entry["unreal_dest"], unreal.SkeletalMesh)
            static_mesh = None if skel_mesh else find_asset_in_paths_or_folder(paths, entry["unreal_dest"], unreal.StaticMesh)
            if not skel_mesh and not static_mesh:
                unreal.log_warning(f"[ABFP] No mesh found after importing {entry['id']}")
                continue

            idle_anim = find_asset_in_paths_or_folder(paths, entry["unreal_dest"], unreal.AnimSequence, "idle") if skel_mesh else None
            hit_sound = audio_by_id.get(entry.get("hit_sound", ""))
            death_sound = audio_by_id.get(entry.get("death_sound", ""))

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
                ce.set_editor_property("scale", unreal.Vector(*entry.get("scale", [1, 1, 1])))
                ce.set_editor_property("location_offset", unreal.Vector(*entry.get("location_offset", [0, 0, 0])))
                if hit_sound:
                    ce.set_editor_property("hit_sound", hit_sound)
                if death_sound:
                    ce.set_editor_property("death_sound", death_sound)
                creature_entries.append(ce)
        except Exception as exc:
            unreal.log_warning(f"[ABFP] Failed importing creature {entry['id']}: {exc}")

    catalog.set_editor_property("creatures", creature_entries)

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
