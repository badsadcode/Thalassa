"""Imports the dark biome (Scripts/blender/make_dark_biome.py) into /Game/World/DarkBiome, and builds M_Growth.

Reads Saved/DarkBiome/DarkBoulders.fbx and Life.fbx; make them first with:
    python Scripts/build/blender.py Scripts/blender/make_dark_biome.py -- <project>/Saved/DarkBiome

Boulders: Nanite, collision from their real shape, M_Rock.
Life: M_Growth, no collision. Its vertex colours drive the material:
  red   sways in the current, root to tip, each copy on its own beat;
  green glows, pulsing slowly: bioluminescence, the only light here besides the ship's;
  blue  red plume instead of pale tissue.

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript=<this file> -unattended
"""
import os
import unreal

FOLDER = "/Game/World/DarkBiome"
SAVED = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "DarkBiome")
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
BOULDERS = ["SM_DarkBoulder_%02d" % i for i in range(1, 7)]
LIFE = ["SM_%s_%02d" % (kind, i) for kind in ("SeaPen", "TubeWorms", "SeaWhip", "GlowBulb") for i in range(1, 4)]


def import_fbx(name):
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
    task.filename = os.path.join(SAVED, name + ".fbx")
    task.destination_path = FOLDER
    task.replace_existing = True
    task.automated = True
    task.save = False
    task.options = options
    tools.import_asset_tasks([task])


def node(m, cls, x, y, **props):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, b, pin="", out=""):
    mel.connect_material_expressions(a, out, b, pin)


def make_growth():
    path = FOLDER + "/M_Growth"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset("M_Growth", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)

    vc = node(m, unreal.MaterialExpressionVertexColor, -1400, 0)
    sway = node(m, unreal.MaterialExpressionComponentMask, -1200, -200, r=True, g=False, b=False, a=False)
    glow = node(m, unreal.MaterialExpressionComponentMask, -1200, 0, r=False, g=True, b=False, a=False)
    plume = node(m, unreal.MaterialExpressionComponentMask, -1200, 200, r=False, g=False, b=True, a=False)
    for mask in (sway, glow, plume):
        link(vc, mask)

    # Pale tissue or red plume.
    pale = node(m, unreal.MaterialExpressionConstant3Vector, -1000, 300, constant=unreal.LinearColor(0.55, 0.52, 0.46, 1))
    red = node(m, unreal.MaterialExpressionConstant3Vector, -1000, 450, constant=unreal.LinearColor(0.42, 0.035, 0.03, 1))
    colour = node(m, unreal.MaterialExpressionLinearInterpolate, -700, 300)
    link(pale, colour, "A")
    link(red, colour, "B")
    link(plume, colour, "Alpha")
    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = node(m, unreal.MaterialExpressionConstant, -700, 500, r=0.55)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # Each copy has its own beat, from its per-instance random number.
    time = node(m, unreal.MaterialExpressionTime, -1400, 700)
    rand = node(m, unreal.MaterialExpressionPerInstanceRandom, -1400, 850)
    phase = node(m, unreal.MaterialExpressionMultiply, -1200, 850, const_b=6.283)
    link(rand, phase, "A")

    def wave(speed, cls, y):
        t = node(m, unreal.MaterialExpressionMultiply, -1100, y, const_b=speed)
        link(time, t, "A")
        a = node(m, unreal.MaterialExpressionAdd, -950, y)
        link(t, a, "A")
        link(phase, a, "B")
        w = node(m, cls, -800, y, period=6.283)
        link(a, w)
        return w

    # Glow: cold blue-green, pulsing slowly between half and full.
    pulse = wave(1.1, unreal.MaterialExpressionSine, 700)
    half = node(m, unreal.MaterialExpressionMultiply, -650, 700, const_b=0.25)
    link(pulse, half, "A")
    lift = node(m, unreal.MaterialExpressionAdd, -500, 700, const_b=0.75)
    link(half, lift, "A")
    glow_colour = node(m, unreal.MaterialExpressionConstant3Vector, -650, 850, constant=unreal.LinearColor(0.05, 0.9, 0.75, 1))
    g1 = node(m, unreal.MaterialExpressionMultiply, -400, 750)
    link(glow_colour, g1, "A")
    link(lift, g1, "B")
    g2 = node(m, unreal.MaterialExpressionMultiply, -250, 750)
    link(g1, g2, "A")
    link(glow, g2, "B")
    g3 = node(m, unreal.MaterialExpressionMultiply, -100, 750, const_b=8.0)
    link(g2, g3, "A")
    mel.connect_material_property(g3, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    # Sway: a slow circle in the current, scaled by how far from the root a point is.
    sx = wave(0.8, unreal.MaterialExpressionSine, 1000)
    sy = wave(0.6, unreal.MaterialExpressionCosine, 1150)
    xy = node(m, unreal.MaterialExpressionAppendVector, -650, 1050)
    link(sx, xy, "A")
    link(sy, xy, "B")
    zero = node(m, unreal.MaterialExpressionConstant, -650, 1200, r=0.0)
    xyz = node(m, unreal.MaterialExpressionAppendVector, -500, 1100)
    link(xy, xyz, "A")
    link(zero, xyz, "B")
    amount = node(m, unreal.MaterialExpressionMultiply, -350, 1100)
    link(xyz, amount, "A")
    link(sway, amount, "B")
    cm = node(m, unreal.MaterialExpressionMultiply, -200, 1100, const_b=14.0)  # Up to 14 cm at the tip.
    link(amount, cm, "A")
    mel.connect_material_property(cm, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    mel.recompile_material(m)
    eal.save_asset(path)
    return m


import_fbx("DarkBoulders")
import_fbx("Life")
rock = unreal.load_asset("/Game/World/Materials/M_Rock")
growth = make_growth()
done = 0
for name in BOULDERS + LIFE:
    path = FOLDER + "/" + name
    mesh = unreal.load_asset(path)
    if not mesh:
        unreal.log_error("THALASSA: %s did not import" % name)
        continue
    is_rock = name in BOULDERS
    mesh.set_material(0, rock if is_rock else growth)
    if is_rock:
        nanite = mesh.get_editor_property("nanite_settings")
        nanite.set_editor_property("enabled", True)
        mesh.set_editor_property("nanite_settings", nanite)
        body = mesh.get_editor_property("body_setup")
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        mesh.set_editor_property("body_setup", body)
    eal.save_asset(path)
    done += 1
unreal.log("THALASSA: imported %d of %d dark biome meshes, built M_Growth" % (done, len(BOULDERS) + len(LIFE)))
