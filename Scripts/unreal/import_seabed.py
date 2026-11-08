"""Imports the seabed made by Scripts/blender/make_seabed.py as /Game/World/Seabed/SM_Seabed, with its material.

Replaces the mesh if it exists. The FBX is read from Saved/Seabed/SM_Seabed.fbx; make it first with:
    python Scripts/build/blender.py Scripts/blender/make_seabed.py -- <project>/Saved/Seabed/SM_Seabed.fbx

Collision is the terrain itself (complex as simple): it never moves and nobody stands on it aboard the ship,
so the walkable-shell rule about boxes does not apply. Drawn with Nanite, for its 720,000 triangles.

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript=<this file> -unattended -nullrhi
"""
import os
import unreal

FOLDER = "/Game/World/Seabed"
MESH = FOLDER + "/SM_Seabed"
MATERIAL = FOLDER + "/M_Seabed"
FBX = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Seabed", "SM_Seabed.fbx")

tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary


def make_material():
    if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL):
        return unreal.load_asset(MATERIAL)
    material = tools.create_asset("M_Seabed", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("used_with_nanite", True)  # The seabed is a Nanite mesh.
    # Sand and rock come from the vertex colours. Two scales of world-space noise break up the flat colour.
    colour = mel.create_material_expression(material, unreal.MaterialExpressionVertexColor, -900, 0)
    broad = mel.create_material_expression(material, unreal.MaterialExpressionNoise, -900, 250)
    broad.set_editor_property("scale", 0.0008)
    broad.set_editor_property("output_min", 0.7)
    broad.set_editor_property("output_max", 1.15)
    fine = mel.create_material_expression(material, unreal.MaterialExpressionNoise, -900, 500)
    fine.set_editor_property("scale", 0.01)
    fine.set_editor_property("output_min", 0.85)
    fine.set_editor_property("output_max", 1.05)
    a = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -500, 100)
    b = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -300, 200)
    mel.connect_material_expressions(colour, "", a, "A")
    mel.connect_material_expressions(broad, "", a, "B")
    mel.connect_material_expressions(a, "", b, "A")
    mel.connect_material_expressions(fine, "", b, "B")
    mel.connect_material_property(b, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -300, 400)
    rough.set_editor_property("r", 0.95)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(MATERIAL)
    return material


options = unreal.FbxImportUI()
options.import_mesh = True
options.import_materials = False
options.import_textures = False
options.import_animations = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
data = options.static_mesh_import_data
data.combine_meshes = True
data.auto_generate_collision = False
data.generate_lightmap_u_vs = False
data.vertex_color_import_option = unreal.VertexColorImportOption.REPLACE

task = unreal.AssetImportTask()
task.filename = FBX
task.destination_path = FOLDER
task.destination_name = "SM_Seabed"
task.replace_existing = True
task.automated = True
task.save = False
task.options = options
tools.import_asset_tasks([task])

mesh = unreal.load_asset(MESH)
if not mesh:
    raise RuntimeError("THALASSA: seabed import failed: " + FBX)

mesh.set_material(0, make_material())
nanite = mesh.get_editor_property("nanite_settings")
nanite.set_editor_property("enabled", True)
mesh.set_editor_property("nanite_settings", nanite)
body = mesh.get_editor_property("body_setup")
body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
mesh.set_editor_property("body_setup", body)
unreal.EditorAssetLibrary.save_asset(MESH)

bounds = mesh.get_bounds()
unreal.log("THALASSA: imported %s, bounds %s +- %s" % (MESH, bounds.origin, bounds.box_extent))
