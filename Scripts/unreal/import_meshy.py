"""Imports Meshy AI models (FBX with PNG textures) from Saved/MeshySlim/<model folder>/ into /Game/World/Meshy.

Meshy exports one folder per model: <name>.fbx, <name>.png (colour), <name>_normal.png, <name>_roughness.png and
<name>_metallic.png. Unzip each into Saved/Meshy (which git ignores), then slim it to the game's budget (at most
30,000 triangles, textures at most 2K, origin at the middle of its base):
    python Scripts/build/blender.py Scripts/blender/slim_meshy.py -- Saved/Meshy/<folder> Saved/MeshySlim/<folder>
Each becomes:
  - SM_Meshy_<Name>: the mesh, drawn with Nanite, its collision the mesh itself;
  - T_Meshy_<Name>_Colour / _Normal / _Roughness: the textures (metallic is left out: rock isn't metal);
  - MI_Meshy_<Name>: an instance of M_MeshyRock with those textures.
M_MeshyRock can draw a model two ways, mixed by its ScanBlend parameter (0 to 1):
  0  Meshy's own colour texture, cooled a little towards the seabed's palette;
  1  our scanned dark rock (the same as M_Rock), projected by world position, shaded by Meshy's colour texture's
     light and dark, so its cracks and hollows stay. The default: Meshy's own colours were not liked.
Either way it uses Meshy's normal and roughness maps, and shows the sonar ping's ring like every rock. It is marked
for Nanite and instanced meshes, which the open ocean's clutter needs.

<Name> is taken from the folder: Meshy_AI_Weathered_Stone_Arch_20261109_texture_fbx becomes WeatheredStoneArch.

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript=<this file> -unattended -nullrhi
"""
import glob
import os
import re
import unreal

FOLDER = "/Game/World/Meshy"
SOURCE = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "MeshySlim")
SCAN = "/Game/World/Textures/dark_rock_02"
PULSE = "/Game/Ship/Sonar/MPC_SonarPulse"

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# The sonar ping's ring, as on the other rocks (create_ocean_materials.py, PULSE_HLSL).
PULSE_HLSL = """
float d = distance(P, O.xyz);
float front = saturate(1 - abs(d - R) / 400);
front *= front;
float behind = (R - d) / 4000;
float trail = (behind > 0 && behind < 1) ? (1 - behind) * (1 - behind) : 0;
float c = frac(P.z / 500);
float contour = saturate((abs(c * 2 - 1) - 0.93) * 14);
return (front * 2.5 + trail * (0.04 + contour * 1.2)) * S * float3(0.25, 0.9, 1.0);
"""


def model_name(folder):
    base = os.path.basename(folder)
    match = re.match(r"Meshy_AI_(.+?)_\d{8}", base)
    words = (match.group(1) if match else base).split("_")
    return "".join(w[:1].upper() + w[1:] for w in words if w)


def make_parent():
    path = FOLDER + "/M_MeshyRock"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset("M_MeshyRock", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("used_with_nanite", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)

    def node(cls, x, y, **props):
        e = mel.create_material_expression(m, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def texture(name, y, sampler):
        t = node(unreal.MaterialExpressionTextureSampleParameter2D, -800, y, parameter_name=name, sampler_type=sampler)
        return t

    colour = texture("Colour", -300, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal = texture("Normal", 0, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    rough = texture("Roughness", 300, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    tint = node(unreal.MaterialExpressionVectorParameter, -800, -500, parameter_name="Tint", default_value=unreal.LinearColor(0.78, 0.84, 0.86, 1.0))
    own = node(unreal.MaterialExpressionMultiply, -400, -350)
    mel.connect_material_expressions(colour, "RGB", own, "A")
    mel.connect_material_expressions(tint, "", own, "B")

    # Our scanned rock, projected from three sides by world position, shaded by Meshy's light and dark.
    scan_texture = unreal.load_asset(SCAN + "/T_DarkRock02_C")
    project = node(unreal.MaterialExpressionMaterialFunctionCall, -800, -800)
    project.set_editor_property("material_function", unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture"))
    mel.connect_material_expressions(node(unreal.MaterialExpressionTextureObject, -1100, -850, texture=scan_texture), "", project, "TextureObject")
    mel.connect_material_expressions(node(unreal.MaterialExpressionConstant3Vector, -1100, -750, constant=unreal.LinearColor(350.0, 350.0, 350.0, 1.0)), "", project, "TextureSize")
    luma = node(unreal.MaterialExpressionDotProduct, -600, -650)
    mel.connect_material_expressions(colour, "RGB", luma, "A")
    mel.connect_material_expressions(node(unreal.MaterialExpressionConstant3Vector, -800, -600, constant=unreal.LinearColor(0.3, 0.59, 0.11, 1.0)), "", luma, "B")
    shade = node(unreal.MaterialExpressionMultiply, -450, -650)
    mel.connect_material_expressions(luma, "", shade, "A")
    mel.connect_material_expressions(node(unreal.MaterialExpressionConstant, -600, -560, r=2.6), "", shade, "B")
    scanned = node(unreal.MaterialExpressionMultiply, -300, -750)
    mel.connect_material_expressions(project, "XYZ Texture", scanned, "A")
    mel.connect_material_expressions(shade, "", scanned, "B")
    blend = node(unreal.MaterialExpressionScalarParameter, -300, -500, parameter_name="ScanBlend", default_value=1.0)
    base = node(unreal.MaterialExpressionLinearInterpolate, -150, -400)
    mel.connect_material_expressions(own, "", base, "A")
    mel.connect_material_expressions(scanned, "", base, "B")
    mel.connect_material_expressions(blend, "", base, "Alpha")
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(rough, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(node(unreal.MaterialExpressionConstant, -400, 450, r=0.35), "", unreal.MaterialProperty.MP_SPECULAR)

    mpc = unreal.load_asset(PULSE)
    if mpc:
        custom = node(unreal.MaterialExpressionCustom, -300, 650)
        custom.set_editor_property("code", PULSE_HLSL)
        custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        custom.set_editor_property("description", "Sonar ping ring")
        pins = []
        for n in ["P", "O", "R", "S"]:
            i = unreal.CustomInput()
            i.set_editor_property("input_name", n)
            pins.append(i)
        custom.set_editor_property("inputs", pins)
        mel.connect_material_expressions(node(unreal.MaterialExpressionWorldPosition, -700, 600), "", custom, "P")
        for pin, param, y in (("O", "PulseOrigin", 650), ("R", "PulseRadius", 700), ("S", "PulseStrength", 750)):
            p = node(unreal.MaterialExpressionCollectionParameter, -700, y)
            p.set_editor_property("collection", mpc)
            p.set_editor_property("parameter_name", param)
            mel.connect_material_expressions(p, "", custom, pin)
        mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    eal.save_asset(path)
    return m


def import_texture(file, target_folder, name, kind):
    task = unreal.AssetImportTask()
    task.filename = file
    task.destination_path = target_folder
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    tools.import_asset_tasks([task])
    texture = unreal.load_asset(target_folder + "/" + name)
    if not texture:
        unreal.log_error("THALASSA: texture %s did not import" % file)
        return None
    if kind == "normal":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("srgb", False)
    elif kind == "linear":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
        texture.set_editor_property("srgb", False)
    eal.save_asset(target_folder + "/" + name)
    return texture


def import_model(folder, parent):
    name = model_name(folder)
    fbx = glob.glob(os.path.join(folder, "*.fbx"))
    pngs = glob.glob(os.path.join(folder, "*.png"))
    if not fbx:
        unreal.log_warning("THALASSA: no FBX in " + folder)
        return
    target = FOLDER + "/" + name

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
    task = unreal.AssetImportTask()
    task.filename = fbx[0]
    task.destination_path = target
    task.destination_name = "SM_Meshy_" + name
    task.replace_existing = True
    task.automated = True
    task.save = False
    task.options = options
    tools.import_asset_tasks([task])
    imported = [p.split(".")[0] for p in task.get_editor_property("imported_object_paths")]
    mesh_path = target + "/SM_Meshy_" + name
    meshes = [p for p in imported if isinstance(unreal.load_asset(p), unreal.StaticMesh)]
    if meshes and meshes[0] != mesh_path:
        if eal.does_asset_exist(mesh_path):
            eal.delete_asset(mesh_path)
        eal.rename_asset(meshes[0], mesh_path)
    mesh = unreal.load_asset(mesh_path)
    if not mesh:
        unreal.log_error("THALASSA: %s did not import (got %s)" % (name, imported))
        return

    colour = normal = rough = None
    for png in pngs:
        lower = png.lower()
        if lower.endswith("_normal.png"):
            normal = import_texture(png, target, "T_Meshy_%s_Normal" % name, "normal")
        elif lower.endswith("_roughness.png"):
            rough = import_texture(png, target, "T_Meshy_%s_Roughness" % name, "linear")
        elif lower.endswith("_metallic.png"):
            continue
        else:
            colour = import_texture(png, target, "T_Meshy_%s_Colour" % name, "colour")

    mi_path = target + "/MI_Meshy_" + name
    if eal.does_asset_exist(mi_path):
        eal.delete_asset(mi_path)
    mi = tools.create_asset("MI_Meshy_" + name, target, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, parent)
    for param, tex in (("Colour", colour), ("Normal", normal), ("Roughness", rough)):
        if tex:
            mel.set_material_instance_texture_parameter_value(mi, param, tex)
    eal.save_asset(mi_path)

    for slot in range(len(mesh.get_editor_property("static_materials"))):
        mesh.set_material(slot, mi)
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.set_editor_property("enabled", True)
    mesh.set_editor_property("nanite_settings", nanite)
    body = mesh.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    eal.save_asset(mesh_path)
    box = mesh.get_bounding_box()
    triangles = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_number_verts(mesh, 0)
    unreal.log("THALASSA: Meshy %s: %.0f x %.0f x %.0f cm, %d vertices, textures %s" % (
        name, box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z, triangles,
        [t.get_name() for t in (colour, normal, rough) if t]))


parent = make_parent()
folders = [f for f in glob.glob(os.path.join(SOURCE, "*")) if os.path.isdir(f)]
for folder in folders:
    import_model(folder, parent)
unreal.log("THALASSA: imported %d Meshy models into %s" % (len(folders), FOLDER))
