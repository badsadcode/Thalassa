"""Imports the rock formations made by Scripts/blender/make_formations.py into /Game/World/Formations.

Reads Saved/Formations/Formations.fbx; make it first with:
    blender --background --factory-startup --python Scripts/blender/make_formations.py -- <project>/Saved/Formations/Formations.fbx

Each object becomes its own static mesh, replacing any that exist. They use the seabed's material, which
draws their vertex colours: the strata and mineral staining. Drawn with Nanite. Collision is the rock itself
(complex as simple): formations never move, and the ship's hull check must meet their real shape.

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript=<this file> -unattended -nullrhi
"""
import os
import unreal

FOLDER = "/Game/World/Formations"
FBX = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Formations", "Formations.fbx")
NAMES = ["SM_SlabTower_%02d" % i for i in range(1, 7)] + ["SM_SlabFragment_%02d" % i for i in range(1, 6)] + ["SM_Monolith_Bent"]

options = unreal.FbxImportUI()
options.import_mesh = True
options.import_materials = False
options.import_textures = False
options.import_animations = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
data = options.static_mesh_import_data
data.combine_meshes = False
data.auto_generate_collision = False
data.generate_lightmap_u_vs = False
data.vertex_color_import_option = unreal.VertexColorImportOption.REPLACE

task = unreal.AssetImportTask()
task.filename = FBX
task.destination_path = FOLDER
task.replace_existing = True
task.automated = True
task.save = False
task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

material = unreal.load_asset("/Game/World/Seabed/M_Seabed")
found = 0
for name in NAMES:
    mesh = unreal.load_asset(FOLDER + "/" + name)
    if not mesh:
        unreal.log_error("THALASSA: %s did not import" % name)
        continue
    found += 1
    mesh.set_material(0, material)
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.set_editor_property("enabled", True)
    mesh.set_editor_property("nanite_settings", nanite)
    body = mesh.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    mesh.set_editor_property("body_setup", body)
    unreal.EditorAssetLibrary.save_asset(FOLDER + "/" + name)
unreal.log("THALASSA: imported %d of %d formations into %s" % (found, len(NAMES), FOLDER))
