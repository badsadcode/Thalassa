"""Imports the CC0 scanned rock models (Scripts/build/fetch_textures.py) into /Game/World/Rocks/<asset>/.

Each glTF brings its mesh, its scanned textures and a material. Meshes get Nanite and collision from their
real shape. Colours are pulled toward the cold underwater palette, since these were scanned on land in
sunlight. Replaces assets that exist. Logs each mesh's size, for the scatter rules in create_test_map.py.

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript=<this file> -unattended
"""
import os
import unreal

SAVED = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Models")
ROCKS = "/Game/World/Rocks"
MODELS = ["moon_rock_01", "moon_rock_03", "moon_rock_05", "rock_07", "rock_09", "namaqualand_boulder_03",
          "namaqualand_boulder_05", "coast_rocks_05"]
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

tasks = []
for asset in MODELS:
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SAVED, asset, asset + ".gltf")
    task.destination_path = ROCKS + "/" + asset
    task.replace_existing = True
    task.automated = True
    task.save = True
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

for asset in MODELS:
    folder = ROCKS + "/" + asset
    meshes = [a for a in eal.list_assets(folder, recursive=True) if isinstance(unreal.load_asset(a), unreal.StaticMesh)]
    if not meshes:
        unreal.log_error("THALASSA: no mesh imported for " + asset)
        continue
    for path in meshes:
        mesh = unreal.load_asset(path)
        nanite = mesh.get_editor_property("nanite_settings")
        nanite.set_editor_property("enabled", True)
        mesh.set_editor_property("nanite_settings", nanite)
        body = mesh.get_editor_property("body_setup")
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        mesh.set_editor_property("body_setup", body)
        # Cool the scanned colours: sunlit land rock, now 400 m down.
        for slot in range(mesh.get_num_sections(0)):
            material = mesh.get_material(slot)
            if isinstance(material, unreal.MaterialInstanceConstant):
                names = [str(n) for n in mel.get_vector_parameter_names(material)]
                for name in names:
                    if "basecolor" in name.lower() and "factor" in name.lower():
                        mel.set_material_instance_vector_parameter_value(material, name, unreal.LinearColor(0.62, 0.66, 0.68, 1.0))
                        eal.save_asset(material.get_path_name())
                unreal.log("THALASSA: %s material %s parameters %s" % (asset, material.get_name(), names))
        eal.save_asset(path)
        if "_LOD" in mesh.get_name() and not mesh.get_name().endswith("_LOD0"):
            eal.delete_asset(path)  # Nanite needs no lower-detail copies.
            continue
        b = mesh.get_bounds()
        unreal.log("THALASSA: %s -> %s, size %.0f x %.0f x %.0f cm, origin %s" % (
            asset, mesh.get_name(), b.box_extent.x * 2, b.box_extent.y * 2, b.box_extent.z * 2, b.origin))
