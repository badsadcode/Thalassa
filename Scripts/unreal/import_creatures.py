"""Imports the creatures made by Scripts/blender/make_creatures.py into /Game/Creatures, and builds their materials.

Reads Saved/Creatures/Creatures.fbx; make it first with:
    python Scripts/build/blender.py Scripts/blender/make_creatures.py -- <project>/Saved/Creatures/Creatures.fbx

M_Creature draws the fish, the gliders and the leviathan; M_Jelly (see-through, unlit, glowing) the jellyfish.
Both take the skin from the vertex colours, glow where UV channel 4 marks it, and move in the vertex shader (world
position offset), so moving a thousand animals costs the CPU only their positions. What each mesh carries for this
is described in make_creatures.py. Per instance, three numbers (PerInstanceCustomData):
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
NAMES = ["SM_Glider", "SM_Fish_Dart", "SM_Fish_Disc", "SM_Fish_Lantern", "SM_Fish_Grouper", "SM_Fish_Eel", "SM_Fish_Cruiser",
         "SM_Jelly_Giant", "SM_Jelly_Glow", "SM_Jelly_Mote", "SM_Leviathan_Head", "SM_Leviathan_Body", "SM_Leviathan_Tail"]

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# The motion, in the mesh's own centimetres. A: where the vertex is on the moving part (0 to 1). Sz: the creature's
# size in metres. M: how it moves (see make_creatures.py). P: the vertex's position in the mesh. T: time.
# Ph, Rt: phase and rate.
MOTION_HLSL = """
float3 o = float3(0, 0, 0);
float s = Sz * 100;
if (M > 2.5)
{
    // Not at all: the leviathan's pieces move by where they are put.
}
else if (M > 1.5)
{
    // A jellyfish. The bell: a slow pulse, the rim drawing in most, then relaxing.
    float w = 6.2832 / (1.3 * sqrt(max(Sz, 0.2))) * Rt;
    float pulse = 0.5 + 0.5 * sin(T * w + Ph);
    if (A < 0.15)
    {
        float k = A / 0.15;
        o.xy = -P.xy * 0.14 * pulse * k;
        o.z = s * 0.03 * pulse * k;
    }
    else
    {
        // The tentacles: trailing, swaying slowly, more towards their tips.
        float k = (A - 0.15) / 0.85;
        float reach = pow(k, 1.5) * s * 0.35;
        o.x = sin(T * 0.45 * Rt + Ph + k * 3.0) * reach;
        o.y = cos(T * 0.37 * Rt + Ph * 1.3 + k * 2.5) * reach;
        o.z = s * 0.03 * pulse;
    }
}
else if (M > 0.5)
{
    // Wings: a slow stroke, the tips lagging and travelling furthest. About 4 s a beat for a 5 m animal.
    float w = 6.2832 / (0.8 * sqrt(Sz)) * Rt;
    o.z = sin(T * w + Ph - A * 1.3) * pow(A, 1.6) * s * 0.11;
}
else
{
    // A body: a wave running back along it, growing towards the tail. Small fish beat fast, big ones slowly.
    float w = 6.2832 * 1.6 / sqrt(Sz) * Rt;
    o.y = sin(T * w + Ph - A * 5.0) * A * A * s * 0.08;
}
return o;
"""

# The glow's colour: a bright skin glows its own colour (the leviathan's amber eyes, the jellyfish); a dark one
# glows cold blue-green (the lanterns' spots, the gliders' wingtips).
GLOW_HLSL = """
float l = max(V.r, max(V.g, V.b));
return l > 0.3 ? V / l : float3(0.15, 0.85, 1.0);
"""


def make_material(name, jelly=False):
    """M_Creature (opaque, lit) for fish, gliders and the leviathan; M_Jelly (see-through, glowing) for jellyfish."""
    path = FOLDER + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    if jelly:
        m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

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

    def custom(code, inputs, description, x, y):
        c = node(unreal.MaterialExpressionCustom, x, y)
        c.set_editor_property("code", code)
        c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        c.set_editor_property("description", description)
        pins = []
        for n in inputs:
            i = unreal.CustomInput()
            i.set_editor_property("input_name", n)
            pins.append(i)
        c.set_editor_property("inputs", pins)
        return c

    def binary(cls, a, b, x, y):
        e = node(cls, x, y)
        mel.connect_material_expressions(a, "", e, "A")
        mel.connect_material_expressions(b, "", e, "B")
        return e

    def mul(a, b, x, y):
        return binary(unreal.MaterialExpressionMultiply, a, b, x, y)

    def add(a, b, x, y):
        return binary(unreal.MaterialExpressionAdd, a, b, x, y)

    def const(v, x, y):
        return node(unreal.MaterialExpressionConstant, x, y, r=v)

    vertex = node(unreal.MaterialExpressionVertexColor, -1300, -400)
    glow_mask = uv(4, -150)
    glow_colour = custom(GLOW_HLSL, ["V"], "Glow colour", -900, -250)
    mel.connect_material_expressions(vertex, "", glow_colour, "V")
    # How strongly it glows: a little always, much more in a flash.
    strength = add(const(1.5 if jelly else 0.6, -900, 0), mul(custom_data(2, 60), const(12.0, -1000, 120), -800, 60), -700, 0)
    glow = mul(mul(glow_colour, glow_mask, -600, -200), strength, -400, -150)

    if jelly:
        # See-through, brighter and more solid at the edges, as a jellyfish is, glowing where it glows.
        fresnel = node(unreal.MaterialExpressionFresnel, -900, -500, exponent=2.5)
        skin = mul(vertex, add(const(0.2, -900, -580), fresnel, -700, -500), -500, -450)
        mel.connect_material_property(add(skin, glow, -250, -300), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        solid = add(mul(fresnel, const(0.45, -900, -700), -700, -650), add(const(0.1, -900, -820), mul(glow_mask, const(0.5, -1000, -880), -800, -860), -650, -800), -500, -650)
        clamp = node(unreal.MaterialExpressionSaturate, -300, -650)
        mel.connect_material_expressions(solid, "", clamp, "")
        mel.connect_material_property(clamp, "", unreal.MaterialProperty.MP_OPACITY)
    else:
        mel.connect_material_property(vertex, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(const(0.38, -400, -300), "", unreal.MaterialProperty.MP_ROUGHNESS)
        mel.connect_material_property(const(0.6, -400, -250), "", unreal.MaterialProperty.MP_SPECULAR)
        mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    motion = custom(MOTION_HLSL, ["A", "Sz", "M", "P", "T", "Ph", "Rt"], "Creature motion", -500, 300)
    mel.connect_material_expressions(uv(1, 200), "", motion, "A")
    mel.connect_material_expressions(uv(2, 280), "", motion, "Sz")
    mel.connect_material_expressions(uv(3, 360), "", motion, "M")
    mel.connect_material_expressions(node(unreal.MaterialExpressionLocalPosition, -1000, 400), "", motion, "P")
    mel.connect_material_expressions(node(unreal.MaterialExpressionTime, -1000, 440), "", motion, "T")
    mel.connect_material_expressions(custom_data(0, 520), "", motion, "Ph")
    rate = node(unreal.MaterialExpressionMax, -800, 600)
    mel.connect_material_expressions(custom_data(1, 600), "", rate, "A")
    mel.connect_material_expressions(const(0.2, -1000, 680), "", rate, "B")
    mel.connect_material_expressions(rate, "", motion, "Rt")

    world = node(unreal.MaterialExpressionTransform, -250, 300,
                 transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
                 transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    mel.connect_material_expressions(motion, "", world, "")
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
unreal.log("THALASSA: imported %d objects" % len(imported))
# A file with one mesh in it is imported under the file's name, not the mesh's.
if len(NAMES) == 1 and imported:
    source = imported[0].split(".")[0]
    target = FOLDER + "/" + NAMES[0]
    if source != target:
        if eal.does_asset_exist(target):
            eal.delete_asset(target)
        eal.rename_asset(source, target)

creature = make_material("M_Creature")
jelly = make_material("M_Jelly", jelly=True)
found = 0
for name in NAMES:
    mesh = unreal.load_asset(FOLDER + "/" + name)
    if not mesh:
        unreal.log_error("THALASSA: %s did not import" % name)
        continue
    found += 1
    mesh.set_material(0, jelly if name.startswith("SM_Jelly") else creature)
    body = mesh.get_editor_property("body_setup")
    if body:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    box = mesh.get_bounding_box()
    channels = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_num_uv_channels(mesh, 0)
    unreal.log("THALASSA: %s, %d UV channels, %.0f by %.0f by %.0f cm" % (name, channels, box.max.x - box.min.x, box.max.y - box.min.y, box.max.z - box.min.z))
    eal.save_asset(FOLDER + "/" + name)
unreal.log("THALASSA: imported %d of %d creatures into %s" % (found, len(NAMES), FOLDER))
