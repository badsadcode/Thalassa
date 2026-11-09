"""Creates /Game/Ship/Damage/M_BreachSpray: the look of water bursting in through a breach in the hull.

See-through, unlit and pale blue-white, so it shows even in the dark red of the alarm lights; brighter and more
solid towards its middle (fresnel), with a fast flicker running along it, like a jet of water catching the light.
UThalassaShipDamageComponent puts it on the cone it draws at each breach.

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript=<this file> -unattended -nullrhi
"""
import unreal

FOLDER = "/Game/Ship/Damage"
PATH = FOLDER + "/M_BreachSpray"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

if eal.does_asset_exist(PATH):
    eal.delete_asset(PATH)
m = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_BreachSpray", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("two_sided", True)


def node(cls, x, y, **props):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


# A flicker running along the jet: noise in the mesh's own space, moving with time.
custom = node(unreal.MaterialExpressionCustom, -600, 0)
custom.set_editor_property("code", "float n = frac(sin(dot(floor(P.xy * 0.08 + float2(0, T * 30)), float2(12.9898, 78.233))) * 43758.5453);\nreturn 0.6 + 0.4 * n;")
custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
pins = []
for name in ["P", "T"]:
    i = unreal.CustomInput()
    i.set_editor_property("input_name", name)
    pins.append(i)
custom.set_editor_property("inputs", pins)
mel.connect_material_expressions(node(unreal.MaterialExpressionLocalPosition, -900, -50), "", custom, "P")
mel.connect_material_expressions(node(unreal.MaterialExpressionTime, -900, 50), "", custom, "T")

fresnel = node(unreal.MaterialExpressionFresnel, -900, 200, exponent=1.5)
middle = node(unreal.MaterialExpressionOneMinus, -700, 200)
mel.connect_material_expressions(fresnel, "", middle, "")

colour = node(unreal.MaterialExpressionConstant3Vector, -600, -200, constant=unreal.LinearColor(0.6, 0.9, 1.0, 1.0))
bright = node(unreal.MaterialExpressionMultiply, -400, -150)
mel.connect_material_expressions(colour, "", bright, "A")
mel.connect_material_expressions(custom, "", bright, "B")
glow = node(unreal.MaterialExpressionMultiply, -250, -150)
mel.connect_material_expressions(bright, "", glow, "A")
mel.connect_material_expressions(node(unreal.MaterialExpressionConstant, -400, -50, r=2.0), "", glow, "B")
mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

opacity = node(unreal.MaterialExpressionMultiply, -400, 200)
mel.connect_material_expressions(middle, "", opacity, "A")
mel.connect_material_expressions(node(unreal.MaterialExpressionConstant, -600, 300, r=0.65), "", opacity, "B")
mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

mel.recompile_material(m)
eal.save_asset(PATH)
unreal.log("THALASSA: created " + PATH)
