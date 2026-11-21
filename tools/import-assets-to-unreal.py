"""Unreal Engine 5.x Editor Python script for Angband: First Person.

Imports all CC0 assets from assets/raw/ (fetched by tools/fetch-assets.py),
builds PBR Materials from the Poly Haven 2K texture sets (Diffuse, DirectX Normal,
and ARM Ambient Occlusion / Roughness / Metallic), and populates
/Game/Data/DA_DungeonVisualCatalog (UABFPVisualCatalog).

Run inside Unreal Editor:
  Tools -> Execute Python Script -> <repo>/tools/import-assets-to-unreal.py

Or headless via UnrealEditor-Cmd.exe:
  UnrealEditor-Cmd.exe "<project>.uproject" -ExecutePythonScript="<repo>/tools/import-assets-to-unreal.py" -nop4 -unattended
"""

import json
import os
from pathlib import Path

import unreal


def find_repo_root() -> Path:
    script_path = Path(__file__).resolve()
    return script_path.parent.parent


def import_asset(source_file: Path, dest_path: str, asset_name: str = None, is_skeletal: bool = False):
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


def find_asset_of_type(folder_path: str, asset_class, name_hint: str = ""):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = registry.get_assets_by_path(folder_path, recursive=True)
    hint_lower = name_hint.lower()
    fallback = None
    for data in assets:
        obj = data.get_asset()
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
        if paths:
            diff_tex = unreal.EditorAssetLibrary.load_asset(paths[0])
    if nor_file:
        paths = import_asset(nor_file, unreal_tex_folder)
        if paths:
            nor_tex = unreal.EditorAssetLibrary.load_asset(paths[0])
            if isinstance(nor_tex, unreal.Texture2D):
                nor_tex.compression_settings = unreal.TextureCompressionSettings.TC_NORMALMAP
                nor_tex.srgb = False
                nor_tex.post_edit_change()
                unreal.EditorAssetLibrary.save_loaded_asset(nor_tex)
    if arm_file:
        paths = import_asset(arm_file, unreal_tex_folder)
        if paths:
            arm_tex = unreal.EditorAssetLibrary.load_asset(paths[0])
            if isinstance(arm_tex, unreal.Texture2D):
                arm_tex.compression_settings = unreal.TextureCompressionSettings.TC_MASKS
                arm_tex.srgb = False
                arm_tex.post_edit_change()
                unreal.EditorAssetLibrary.save_loaded_asset(arm_tex)

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
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
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
    repo_root = find_repo_root()
    manifest_path = repo_root / "assets" / "manifest.json"
    raw_root = repo_root / "assets" / "raw"

    if not raw_root.exists():
        unreal.log_error(f"assets/raw/ not found at {raw_root}. Run tools/fetch-assets.py first.")
        return

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))

    # 1. Build Poly Haven 2K PBR Materials
    materials_by_name = {}
    for entry in manifest.get("pbr_materials", []):
        tex_dir = raw_root / entry["dest_dir"]
        if not tex_dir.exists():
            continue
        mat_name = entry["unreal_material_name"]
        unreal.log(f"[ABFP] Building PBR material {mat_name}...")
        mat = build_pbr_material(
            mat_name,
            tex_dir,
            f"/Game/Assets/Materials/Textures/{entry['id']}",
            "/Game/Assets/Materials",
        )
        if mat:
            materials_by_name[mat_name] = mat

    # 2. Import Audio Cues
    audio_by_id = {}
    for entry in manifest.get("audio", []):
        src = raw_root / entry["dest"]
        if not src.exists():
            continue
        paths = import_asset(src, entry["unreal_dest"], asset_name=entry["id"])
        if paths:
            sound = unreal.EditorAssetLibrary.load_asset(paths[0])
            if isinstance(sound, unreal.SoundBase):
                audio_by_id[entry["id"]] = sound

    # 3. Create or load DA_DungeonVisualCatalog
    catalog_folder = "/Game/Data"
    catalog_name = "DA_DungeonVisualCatalog"
    catalog_path = f"{catalog_folder}/{catalog_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(catalog_path):
        catalog = unreal.EditorAssetLibrary.load_asset(catalog_path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.ABFPVisualCatalog)
        catalog = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            catalog_name, catalog_folder, unreal.ABFPVisualCatalog, factory
        )

    dungeon_terrain = catalog.get_editor_property("dungeon_terrain")
    town_terrain = catalog.get_editor_property("town_terrain")

    floor_variants = []
    town_floor_variants = []
    wall_variants = []
    vein_variants = []
    pillar_variants = []

    # 4. Import Modular Dungeon Terrain Meshes
    for entry in manifest.get("terrain_meshes", []):
        src = raw_root / entry["dest"]
        if not src.exists():
            continue
        unreal.log(f"[ABFP] Importing terrain mesh {entry['id']}...")
        import_asset(src, entry["unreal_dest"], asset_name=entry["id"], is_skeletal=False)
        mesh = find_asset_of_type(entry["unreal_dest"], unreal.StaticMesh, entry["id"])
        if not mesh:
            continue

        mat_override = materials_by_name.get(entry.get("material_override", ""))
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

    # Reuse floor tile flipped upside-down with subterranean ceiling stone material for ceilings
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
    town_terrain.set_editor_property("floor_variants", town_floor_variants or floor_variants)
    town_terrain.set_editor_property("wall_variants", wall_variants)

    catalog.set_editor_property("dungeon_terrain", dungeon_terrain)
    catalog.set_editor_property("town_terrain", town_terrain)

    # 5. Import Poly Haven Hero Props (Lantern, Sword, Boulder, Goblets)
    item_entries = []
    for entry in manifest.get("polyhaven_models", []):
        model_dir = raw_root / entry["dest_dir"]
        gltf_file = next(model_dir.glob("*.gltf"), None)
        if not gltf_file:
            continue
        unreal.log(f"[ABFP] Importing Poly Haven 3D prop {entry['id']}...")
        import_asset(gltf_file, entry["unreal_dest"], is_skeletal=False)
        if "catalog_item" in entry:
            ci = entry["catalog_item"]
            mesh = find_asset_of_type(entry["unreal_dest"], unreal.StaticMesh)
            if mesh:
                item = unreal.ABFPItemVisualEntry()
                item.set_editor_property("entry_name", ci["entry_name"])
                item.set_editor_property("name_contains", ci["name_contains"])
                item.set_editor_property("static_mesh", mesh)
                item.set_editor_property("scale", unreal.Vector(*ci.get("scale", [1, 1, 1])))
                item.set_editor_property("location_offset", unreal.Vector(*ci.get("location_offset", [0, 0, 8])))
                rot = ci.get("rotation_offset", [0, 0, 0])
                item.set_editor_property("rotation_offset", unreal.Rotator(rot[0], rot[1], rot[2]))
                item.set_editor_property("b_float_and_spin", bool(ci.get("float_and_spin", False)))
                item.set_editor_property("b_emit_subtle_light", bool(ci.get("emit_light", False)))
                if "light_color" in ci:
                    lc = ci["light_color"]
                    item.set_editor_property("accent_light_color", unreal.LinearColor(lc[0], lc[1], lc[2], 1.0))
                item_entries.append(item)

    # 6. Import Floor Item Meshes
    for entry in manifest.get("item_meshes", []):
        src = raw_root / entry["dest"]
        if not src.exists():
            continue
        unreal.log(f"[ABFP] Importing item mesh {entry['id']}...")
        import_asset(src, entry["unreal_dest"], asset_name=entry["id"], is_skeletal=False)
        mesh = find_asset_of_type(entry["unreal_dest"], unreal.StaticMesh, entry["id"])
        if not mesh:
            continue
        item = unreal.ABFPItemVisualEntry()
        item.set_editor_property("entry_name", entry["entry_name"])
        item.set_editor_property("name_contains", entry["name_contains"])
        item.set_editor_property("static_mesh", mesh)
        item.set_editor_property("scale", unreal.Vector(*entry.get("scale", [1, 1, 1])))
        item.set_editor_property("location_offset", unreal.Vector(*entry.get("location_offset", [0, 0, 8])))
        item.set_editor_property("b_float_and_spin", bool(entry.get("float_and_spin", False)))
        item.set_editor_property("b_emit_subtle_light", bool(entry.get("emit_light", False)))
        if "light_color" in entry:
            lc = entry["light_color"]
            item.set_editor_property("accent_light_color", unreal.LinearColor(lc[0], lc[1], lc[2], 1.0))
        item_entries.append(item)

    catalog.set_editor_property("items", item_entries)

    # 7. Import Creatures (Skeletal + Static Fallbacks + Idle Animations + Audio)
    creature_entries = []
    for entry in manifest.get("creatures", []):
        src = raw_root / entry["dest"]
        if not src.exists():
            continue
        unreal.log(f"[ABFP] Importing creature {entry['id']}...")
        import_asset(src, entry["unreal_dest"], is_skeletal=bool(entry.get("rigged", False)))

        skel_mesh = find_asset_of_type(entry["unreal_dest"], unreal.SkeletalMesh)
        static_mesh = None if skel_mesh else find_asset_of_type(entry["unreal_dest"], unreal.StaticMesh)
        if not skel_mesh and not static_mesh:
            unreal.log_warning(f"[ABFP] No mesh found after importing {entry['id']}")
            continue

        idle_anim = find_asset_of_type(entry["unreal_dest"], unreal.AnimSequence, "idle") if skel_mesh else None
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

    catalog.set_editor_property("creatures", creature_entries)

    # 8. Wire Global Audio Cues
    if "footstep_a" in audio_by_id:
        catalog.set_editor_property("footstep_sound", audio_by_id["footstep_a"])
    if "hit" in audio_by_id:
        catalog.set_editor_property("attack_swing_sound", audio_by_id["hit"])
    if "grunt_hurt" in audio_by_id:
        catalog.set_editor_property("player_hit_sound", audio_by_id["grunt_hurt"])

    unreal.EditorAssetLibrary.save_loaded_asset(catalog)
    unreal.log(
        f"[ABFP] Populated {catalog_path}: "
        f"{len(materials_by_name)} PBR materials, {len(floor_variants)} floors, {len(wall_variants)} walls, "
        f"{len(creature_entries)} creature rules, {len(item_entries)} item rules, {len(audio_by_id)} sounds."
    )
    unreal.log("[ABFP] Enable it in client/Config/DefaultGame.ini:")
    unreal.log("  VisualCatalogAsset=/Game/Data/DA_DungeonVisualCatalog.DA_DungeonVisualCatalog")


if __name__ == "__main__":
    main()
