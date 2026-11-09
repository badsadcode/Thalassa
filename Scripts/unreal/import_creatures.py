"""Imports the creatures made by Scripts/blender/make_creatures.py into /Game/Creatures, and builds their material.

Reads Saved/Creatures/Creatures.fbx; make it first with:
    python Scripts/build/blender.py Scripts/blender/make_creatures.py -- <project>/Saved/Creatures/Creatures.fbx

M_Creature draws every creature: the skin from the vertex colours, a glow where UV channel 4 marks it, and
the swimming motion in the vertex shader (world position offset), so moving a hundred animals costs the CPU only
their positions. What each mesh carries for this is described in make_creatures.py. Per instance, three numbers
(PerInstanceCustomData):
  0  phase: where in its stroke this animal is, so a school doesn't beat in time
  1  rate: how fast it beats, 1 normally, faster when it flees
  2  flash: extra glow, when a ping startles it

No collision and no Nanite (world position offset is cheaper without it for small meshes).

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript=<this file> -unattended -nullrhi
"""
import os
import unreal

FOLDER = "/Game/Creatures"
FBX = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Creatures", "Creatures.fbx")
NAMES = ["SM_Glider"]

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# The motion, in the mesh's own centimetres. A: where the vertex is on the moving part (0 to 1). Sz: the creature's
# size in metres. M: 0 a body swaying from side to side, 1 wings flapping. T: time. Ph, Rt: phase and rate.
MOTION_HLSL = """
float3 o = float3(0, 0, 0);
float s = Sz * 100;
if (M > 0.5)
{
    // Wings: a slow stroke, the tips lagging and travelling furthest. About 4 s a beat for a 5 m animal.
    float w = 6.2832 / (0.8 * sqrt(Sz)) * Rt;
    o.z = sin(T * w + Ph - A * 1.3) * pow(A, 1.6) * s * 0.11;
}
else
{
    // A body: a wave running back along it, growing towards the tail.
    float w = 6.2832 * 1.6 / sqrt(Sz) * Rt;
    o.y = sin(T * w + Ph - A * 5.0) * A * A * s * 0.08;
}
return o;
"""


def make_material():
    path = FOLDER + "/M_Creature"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset("M_Creature", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)

    def node(cls, x, y, **props):
        e = mel.create_material_expression(m, cls, x, y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def uv(index, y):
        t = node(unreal.MaterialExpressionTextureCoordinate, -1300, y, coordinate_index=index)
        mask = node(unreal.MaterialExpressionComponentMask, -1000, y, r=True, g=False, b=False, a=False)
        mel.connect_material_expressions(t, "", mask, "")
        return mask

    def custom_data(index, y):
        return node(unreal.MaterialExpressionPerInstanceCustomData, -1000, y, data_index=index)

    vertex = node(unreal.MaterialExpressionVertexColor, -800, -400)
    mel.connect_material_property(vertex, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(node(unreal.MaterialExpressionConstant, -400, -300, r=0.38), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(node(unreal.MaterialExpressionConstant, -400, -250, r=0.6), "", unreal.MaterialProperty.MP_SPECULAR)

    # Glow: cold blue-green light where the alpha marks it, a little always, much more in a flash.
    glow = node(unreal.MaterialExpressionMultiply, -500, -150)
    mel.connect_material_expressions(uv(4, -150), "", glow, "A")
    mel.connect_material_expressions(node(unreal.MaterialExpressionConstant3Vector, -800, -100, constant=unreal.LinearColor(0.15, 0.85, 1.0, 1.0)), "", glow, "B")
    strength = node(unreal.MaterialExpressionAdd, -700, 0)
    mel.connect_material_expressions(node(unreal.MaterialExpressionConstant, -900, 0, r=0.6), "", strength, "A")
    flash = node(unreal.MaterialExpressionMultiply, -800, 60)
    mel.connect_material_expressions(custom_data(2, 60), "", flash, "A")
    mel.connect_material_expressions(node(unreal.MaterialExpressionConstant, -1000, 120, r=12.0), "", flash, "B")
    mel.connect_material_expressions(flash, "", strength, "B")
    emissive = node(unreal.MaterialExpressionMultiply, -300, -100)
    mel.connect_material_expressions(glow, "", emissive, "A")
    mel.connect_material_expressions(strength, "", emissive, "B")
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    custom = node(unreal.MaterialExpressionCustom, -500, 300)
    custom.set_editor_property("code", MOTION_HLSL)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("description", "Creature motion")
    names = ["A", "Sz", "M", "T", "Ph", "Rt"]
    inputs = []
    for name in names:
        i = unreal.CustomInput()
        i.set_editor_property("input_name", name)
        inputs.append(i)
    custom.set_editor_property("inputs", inputs)
    mel.connect_material_expressions(uv(1, 200), "", custom, "A")
    mel.connect_material_expressions(uv(2, 280), "", custom, "Sz")
    mel.connect_material_expressions(uv(3, 360), "", custom, "M")
    mel.connect_material_expressions(node(unreal.MaterialExpressionTime, -1000, 440), "", custom, "T")
    mel.connect_material_expressions(custom_data(0, 520), "", custom, "Ph")
    rate = node(unreal.MaterialExpressionMax, -800, 600)
    mel.connect_material_expressions(custom_data(1, 600), "", rate, "A")
    mel.connect_material_expressions(node(unreal.MaterialExpressionConstant, -1000, 680, r=0.2), "", rate, "B")
    mel.connect_material_expressions(rate, "", custom, "Rt")

    world = node(unreal.MaterialExpressionTransform, -250, 300,
                 transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
                 transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    mel.connect_material_expressions(custom, "", world, "")
    mel.connect_material_property(world, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    mel.recompile_material(m)
    eal.save_asset(path)
    return m


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
tools.import_asset_tasks([task])
imported = list(task.get_editor_property("imported_object_paths"))
unreal.log("THALASSA: imported %s" % imported)
# A file with one mesh in it is imported under the file's name, not the mesh's.
if len(NAMES) == 1 and imported:
    source = imported[0].split(".")[0]
    target = FOLDER + "/" + NAMES[0]
    if source != target:
        if eal.does_asset_exist(target):
            eal.delete_asset(target)
        eal.rename_asset(source, target)

material = make_material()
found = 0
for name in NAMES:
    mesh = unreal.load_asset(FOLDER + "/" + name)
    if not mesh:
        unreal.log_error("THALASSA: %s did not import" % name)
        continue
    found += 1
    mesh.set_material(0, material)
    body = mesh.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    box = mesh.get_bounding_box()
    channels = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_num_uv_channels(mesh, 0)
    unreal.log("THALASSA: %s, %d UV channels, bounds %s to %s" % (name, channels, box.min, box.max))
    eal.save_asset(FOLDER + "/" + name)
unreal.log("THALASSA: imported %d of %d creatures into %s" % (found, len(NAMES), FOLDER))
