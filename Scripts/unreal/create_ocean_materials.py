"""Imports the CC0 textures (Scripts/build/fetch_textures.py) and builds the rock and seabed materials.

  /Game/World/Textures/<asset>/...   the textures: colour, normal, and ambient occlusion / roughness.
  /Game/World/Materials/M_Rock         dark layered rock, tinted by the strata and mineral streaks painted
                                        into each rock's vertex colours. For formations.
  /Game/World/Materials/M_SeabedGround rippled sediment on the flats, coral gravel on slopes.

Both glow where the sonar's active ping passes (MPC_SonarPulse, made by create_sonar_pulse.py, which must run
first): a bright ring at the wave front, and height contours fading out behind it.

The meshes have no texture layouts, so every texture is projected from three sides by world position
(the engine's WorldAlignedTexture and WorldAlignedNormal). Rebuilds the materials each run; textures are
imported only if missing. Assigns them to the seabed and the formations.

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript=<this file> -unattended
"""
import os
import unreal

SAVED = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Textures")
TEX = "/Game/World/Textures"
MAT = "/Game/World/Materials"
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary

# (asset folder, file, Unreal name, kind). Kind: colour, normal_gl (OpenGL: flip green), normal_dx, data.
TEXTURES = [
    ("dark_rock_02", "dark_rock_02_color.jpg", "T_DarkRock02_C", "colour"),
    ("dark_rock_02", "dark_rock_02_normal.jpg", "T_DarkRock02_N", "normal_gl"),
    ("dark_rock_02", "dark_rock_02_arm.jpg", "T_DarkRock02_ARM", "data"),
    ("coral_gravel", "coral_gravel_color.jpg", "T_CoralGravel_C", "colour"),
    ("coral_gravel", "coral_gravel_normal.jpg", "T_CoralGravel_N", "normal_gl"),
    ("coral_gravel", "coral_gravel_arm.jpg", "T_CoralGravel_ARM", "data"),
    ("damp_beach_sand", "damp_beach_sand_color.jpg", "T_DampSand_C", "colour"),
    ("damp_beach_sand", "damp_beach_sand_normal.jpg", "T_DampSand_N", "normal_gl"),
    ("damp_beach_sand", "damp_beach_sand_arm.jpg", "T_DampSand_ARM", "data"),
    ("Ground093C", "Ground093C_2K-JPG_Color.jpg", "T_Ground093C_C", "colour"),
    ("Ground093C", "Ground093C_2K-JPG_NormalDX.jpg", "T_Ground093C_N", "normal_dx"),
    ("Ground093C", "Ground093C_2K-JPG_Roughness.jpg", "T_Ground093C_R", "data"),
]


def import_textures():
    tasks = []
    for folder, file, name, kind in TEXTURES:
        path = "%s/%s/%s" % (TEX, folder, name)
        if eal.does_asset_exist(path):
            continue
        task = unreal.AssetImportTask()
        task.filename = os.path.join(SAVED, folder, file)
        task.destination_path = "%s/%s" % (TEX, folder)
        task.destination_name = name
        task.automated = True
        task.save = False
        tasks.append(task)
    if tasks:
        tools.import_asset_tasks(tasks)
    for folder, file, name, kind in TEXTURES:
        path = "%s/%s/%s" % (TEX, folder, name)
        texture = unreal.load_asset(path)
        if not texture:
            raise RuntimeError("THALASSA: texture did not import: " + path)
        if kind.startswith("normal"):
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            texture.set_editor_property("srgb", False)
            texture.set_editor_property("flip_green_channel", kind == "normal_gl")
        elif kind == "data":
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            texture.set_editor_property("srgb", False)
        eal.save_asset(path)


def tex(name, folder):
    return unreal.load_asset("%s/%s/%s" % (TEX, folder, name))


class Graph:
    """Small helpers for building a material graph from Python."""

    def __init__(self, material):
        self.m = material
        self.y = 0

    def node(self, cls, x=-1200, **props):
        e = mel.create_material_expression(self.m, cls, x, self.y)
        self.y += 140
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(self, a, b, pin="", out=""):
        mel.connect_material_expressions(a, out, b, pin)

    def projected(self, texture, size_cm, normal=False):
        """A texture projected from three sides by world position."""
        fn = "/Engine/Functions/Engine_MaterialFunctions01/Texturing/" + ("WorldAlignedNormal" if normal else "WorldAlignedTexture")
        call = self.node(unreal.MaterialExpressionMaterialFunctionCall, -700)
        call.set_editor_property("material_function", unreal.load_asset(fn))
        obj = self.node(unreal.MaterialExpressionTextureObject, -1100, texture=texture)
        size = self.node(unreal.MaterialExpressionConstant3Vector, -1100, constant=unreal.LinearColor(size_cm, size_cm, size_cm, 1))
        self.link(obj, call, "TextureObject")
        self.link(size, call, "TextureSize")
        return call

    def mul(self, a, b, a_out="", b_out=""):
        m = self.node(unreal.MaterialExpressionMultiply, -400)
        self.link(a, m, "A", a_out)
        self.link(b, m, "B", b_out)
        return m

    def lerp(self, a, b, alpha, a_out="", b_out="", alpha_out=""):
        l = self.node(unreal.MaterialExpressionLinearInterpolate, -300)
        self.link(a, l, "A", a_out)
        self.link(b, l, "B", b_out)
        self.link(alpha, l, "Alpha", alpha_out)
        return l

    def channel(self, e, r=False, g=False, b=False):
        """One channel of an ordinary expression (mask() takes a projected texture's output)."""
        m = self.node(unreal.MaterialExpressionComponentMask, -500, r=r, g=g, b=b, a=False)
        self.link(e, m)
        return m

    def mask(self, e, r=False, g=False, b=False):
        m = self.node(unreal.MaterialExpressionComponentMask, -500, r=r, g=g, b=b, a=False)
        self.link(e, m, "", "XYZ Texture")
        return m

    def two_scales(self, texture, size_cm, normal=False):
        """The same texture at two sizes, mixed by large-scale noise, so its repeat never lines up the same way."""
        near = self.projected(texture, size_cm, normal)
        far = self.projected(texture, size_cm * 2.9, normal)
        mix = self.node(unreal.MaterialExpressionNoise, -700, scale=0.0012, output_min=0.0, output_max=1.0, levels=3)
        return self.lerp(near, far, mix, "XYZ Texture", "XYZ Texture")

    def macro(self, scale, low, high):
        """Large-scale variation in world space, so repeating textures do not show from a distance."""
        return self.node(unreal.MaterialExpressionNoise, -700, scale=scale, output_min=low, output_max=high, levels=4)


PULSE = "/Game/Ship/Sonar/MPC_SonarPulse"

# The ping's ring on the ground, from where the wave front is now (all in world centimetres): a bright band at
# the front, then contours every 5 m of height in the 40 m behind it, fading. Black when no ping is out.
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


def add_pulse(g, m):
    """Wires the ping's ring into the material's emissive colour."""
    mpc = unreal.load_asset(PULSE)
    if not mpc:
        unreal.log_warning("THALASSA: " + PULSE + " is missing; run create_sonar_pulse.py first. No ping ring on " + m.get_name())
        return

    def param(name):
        e = g.node(unreal.MaterialExpressionCollectionParameter, -700)
        e.set_editor_property("collection", mpc)
        e.set_editor_property("parameter_name", name)
        return e

    custom = g.node(unreal.MaterialExpressionCustom, -300)
    custom.set_editor_property("code", PULSE_HLSL)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("description", "Sonar ping ring")
    inputs = []
    for name in ["P", "O", "R", "S"]:
        i = unreal.CustomInput()
        i.set_editor_property("input_name", name)
        inputs.append(i)
    custom.set_editor_property("inputs", inputs)
    g.link(g.node(unreal.MaterialExpressionWorldPosition, -700), custom, "P")
    g.link(param("PulseOrigin"), custom, "O")
    g.link(param("PulseRadius"), custom, "R")
    g.link(param("PulseStrength"), custom, "S")
    mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def fresh_material(name):
    path = MAT + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    material = tools.create_asset(name, MAT, unreal.Material, unreal.MaterialFactoryNew())
    # The seabed and every rock are Nanite meshes. Without this flag a packaged build draws them in the default material.
    material.set_editor_property("used_with_nanite", True)
    return material


def make_rock():
    m = fresh_material("M_Rock")
    g = Graph(m)
    colour = g.projected(tex("T_DarkRock02_C", "dark_rock_02"), 350.0)
    normal = g.projected(tex("T_DarkRock02_N", "dark_rock_02"), 350.0, normal=True)
    arm = g.projected(tex("T_DarkRock02_ARM", "dark_rock_02"), 350.0)
    # The strata and mineral streaks are in the vertex colours (about 0.1 to 0.5): scale them up to a tint.
    vertex = g.node(unreal.MaterialExpressionVertexColor)
    tint = g.mul(vertex, g.node(unreal.MaterialExpressionConstant, r=4.5))
    base = g.mul(colour, tint, "XYZ Texture")
    base = g.mul(base, g.macro(0.0006, 0.75, 1.15))
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "XYZ Texture", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(g.mask(arm, g=True), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(g.mask(arm, r=True), "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    add_pulse(g, m)
    mel.recompile_material(m)
    eal.save_asset(MAT + "/M_Rock")
    return m


def make_seabed():
    m = fresh_material("M_SeabedGround")
    g = Graph(m)
    # Flats: rippled sediment. Slopes: coral gravel. Chosen by how steep the ground is.
    sand_c = g.two_scales(tex("T_Ground093C_C", "Ground093C"), 700.0)
    sand_n = g.two_scales(tex("T_Ground093C_N", "Ground093C"), 700.0, normal=True)
    sand_r = g.two_scales(tex("T_Ground093C_R", "Ground093C"), 700.0)
    grav_c = g.two_scales(tex("T_CoralGravel_C", "coral_gravel"), 350.0)
    grav_n = g.two_scales(tex("T_CoralGravel_N", "coral_gravel"), 350.0, normal=True)
    grav_a = g.two_scales(tex("T_CoralGravel_ARM", "coral_gravel"), 350.0)

    up = g.node(unreal.MaterialExpressionVertexNormalWS)
    up_z = g.node(unreal.MaterialExpressionComponentMask, -500, r=False, g=False, b=True, a=False)
    g.link(up, up_z)
    # Steepness: 0 on the flat, 1 from about 25 degrees, with a noisy edge so the line wanders.
    steep = g.node(unreal.MaterialExpressionOneMinus, -450)
    g.link(up_z, steep)
    widen = g.mul(steep, g.node(unreal.MaterialExpressionConstant, r=9.0))
    wander = g.mul(widen, g.macro(0.003, 0.6, 1.4))
    slope = g.node(unreal.MaterialExpressionSaturate, -250)
    g.link(wander, slope)

    # Deep-water sediment is grey-brown, not beach gold: pull the colours down and cool them.
    cool = g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.62, 0.62, 0.60, 1))
    sand = g.mul(sand_c, cool)
    gravel = g.mul(grav_c, g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.55, 0.55, 0.55, 1)))
    base = g.lerp(sand, gravel, slope)
    base = g.mul(base, g.macro(0.0004, 0.7, 1.15))
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(g.lerp(sand_n, grav_n, slope), "", unreal.MaterialProperty.MP_NORMAL)
    rough_sand = g.channel(sand_r, r=True)
    rough_grav = g.channel(grav_a, g=True)
    mel.connect_material_property(g.lerp(rough_sand, rough_grav, slope), "", unreal.MaterialProperty.MP_ROUGHNESS)
    add_pulse(g, m)
    mel.recompile_material(m)
    eal.save_asset(MAT + "/M_SeabedGround")
    return m


import_textures()
rock = make_rock()
seabed = make_seabed()

unreal.load_asset("/Game/World/Seabed/SM_Seabed").set_material(0, seabed)
eal.save_asset("/Game/World/Seabed/SM_Seabed")
for name in ["SM_SlabTower_%02d" % i for i in range(1, 7)] + ["SM_SlabFragment_%02d" % i for i in range(1, 6)] + ["SM_Monolith_Bent"]:
    path = "/Game/World/Formations/" + name
    unreal.load_asset(path).set_material(0, rock)
    eal.save_asset(path)
unreal.log("THALASSA: built M_Rock and M_SeabedGround, assigned to the seabed and %d formations" % 12)
