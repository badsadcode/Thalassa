"""Creates /Game/Ship/Lights/MLF_Floodlight: the pattern inside the hull floodlights' beams.

A light function: it dims parts of each beam, so the pool of light on the seabed and the beam in the water are
mottled, like a real lamp behind a worn, scratched housing, instead of an even oval. Two layers of noise on the
direction from the lamp: broad blotches and fine grain. The "Offset" vector parameter shifts the pattern, so
each lamp gets its own (AThalassaVessel sets it per lamp). "Strength" (0..1) is how strong the mottling is.

Rebuilt every time it runs, so changes here take effect.

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript=<this file> -unattended
"""
import unreal

FOLDER = "/Game/Ship/Lights"
NAME = "MLF_Floodlight"
PATH = FOLDER + "/" + NAME
mel = unreal.MaterialEditingLibrary

if unreal.EditorAssetLibrary.does_asset_exist(PATH):
    unreal.EditorAssetLibrary.delete_asset(PATH)
material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
material.set_editor_property("material_domain", unreal.MaterialDomain.MD_LIGHT_FUNCTION)

# The direction from the lamp, so the pattern stays the same size at any distance, and moves with the lamp.
light = mel.create_material_expression(material, unreal.MaterialExpressionLightVector, -1400, 0)
direction = mel.create_material_expression(material, unreal.MaterialExpressionNormalize, -1250, 0)
mel.connect_material_expressions(light, "", direction, "")
offset = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -1250, 150)
offset.set_editor_property("parameter_name", "Offset")
offset.set_editor_property("default_value", unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
shifted = mel.create_material_expression(material, unreal.MaterialExpressionAdd, -1100, 50)
mel.connect_material_expressions(direction, "", shifted, "A")
mel.connect_material_expressions(offset, "", shifted, "B")


def noise(x, y, scale, levels):
    node = mel.create_material_expression(material, unreal.MaterialExpressionNoise, x, y)
    node.set_editor_property("scale", scale)
    node.set_editor_property("levels", levels)
    node.set_editor_property("output_min", 0.0)
    node.set_editor_property("output_max", 1.0)
    node.set_editor_property("quality", 2)
    mel.connect_material_expressions(shifted, "", node, "Position")
    return node


# Across a beam the direction changes by about 0.5, so scale 8 gives a few blotches, scale 40 a fine grain.
broad = noise(-900, -100, 8.0, 3)
fine = noise(-900, 150, 40.0, 2)
grain = mel.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -650, 150)
grain.set_editor_property("const_a", 0.8)
grain.set_editor_property("const_b", 1.0)
mel.connect_material_expressions(fine, "", grain, "Alpha")
pattern = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -500, 0)
mel.connect_material_expressions(broad, "", pattern, "A")
mel.connect_material_expressions(grain, "", pattern, "B")

# Strength: 0 is an even beam, 1 lets the darkest blotches fall to a third.
strength = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 250)
strength.set_editor_property("parameter_name", "Strength")
strength.set_editor_property("default_value", 1.0)
floor = mel.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -300, 50)
floor.set_editor_property("const_a", 0.35)
floor.set_editor_property("const_b", 1.0)
mel.connect_material_expressions(pattern, "", floor, "Alpha")
mix = mel.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -100, 100)
mix.set_editor_property("const_a", 1.0)
mel.connect_material_expressions(floor, "", mix, "B")
mel.connect_material_expressions(strength, "", mix, "Alpha")
mel.connect_material_property(mix, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

mel.recompile_material(material)
unreal.EditorAssetLibrary.save_asset(PATH)
unreal.log("THALASSA: created " + PATH)
