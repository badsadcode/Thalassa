"""Creates /Game/Ship/Holo/M_HoloPoint: the glowing points of the holotable's hologram.

Unlit and additive: the points are light, not objects. Each point's colour comes from its instance's custom
data (three floats: red, green, blue), set by UThalassaHoloTableComponent. A slow shimmer runs up through it.
Left alone if it exists.

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript=<this file> -unattended
"""
import unreal

PATH = "/Game/Ship/Holo/M_HoloPoint"
mel = unreal.MaterialEditingLibrary

if unreal.EditorAssetLibrary.does_asset_exist(PATH):
    unreal.log("THALASSA: " + PATH + " already exists; left as it is")
else:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_HoloPoint", "/Game/Ship/Holo", unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("used_with_instanced_static_meshes", True)

    colour = mel.create_material_expression(material, unreal.MaterialExpressionPerInstanceCustomData3Vector, -900, 0)
    colour.set_editor_property("data_index", 0)

    # Shimmer: brightness ripples slowly upward through the hologram.
    time = mel.create_material_expression(material, unreal.MaterialExpressionTime, -1100, 300)
    position = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1100, 450)
    z = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -900, 450)
    z.set_editor_property("r", False)
    z.set_editor_property("g", False)
    z.set_editor_property("b", True)
    mel.connect_material_expressions(position, "", z, "")
    speed = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -900, 300)
    speed.set_editor_property("const_b", 2.5)
    mel.connect_material_expressions(time, "", speed, "A")
    spacing = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -700, 450)
    spacing.set_editor_property("const_b", -0.15)
    mel.connect_material_expressions(z, "", spacing, "A")
    phase = mel.create_material_expression(material, unreal.MaterialExpressionAdd, -550, 350)
    mel.connect_material_expressions(speed, "", phase, "A")
    mel.connect_material_expressions(spacing, "", phase, "B")
    wave = mel.create_material_expression(material, unreal.MaterialExpressionSine, -400, 350)
    mel.connect_material_expressions(phase, "", wave, "")
    shimmer = mel.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -250, 350)
    shimmer.set_editor_property("const_a", 0.75)
    shimmer.set_editor_property("const_b", 1.0)
    remap = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -400, 500)
    remap.set_editor_property("const_b", 0.5)
    mel.connect_material_expressions(wave, "", remap, "A")
    half = mel.create_material_expression(material, unreal.MaterialExpressionAdd, -300, 500)
    half.set_editor_property("const_b", 0.5)
    mel.connect_material_expressions(remap, "", half, "A")
    mel.connect_material_expressions(half, "", shimmer, "Alpha")

    bright = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -500, 0)
    bright.set_editor_property("const_b", 4.0)
    mel.connect_material_expressions(colour, "", bright, "A")
    out = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -100, 100)
    mel.connect_material_expressions(bright, "", out, "A")
    mel.connect_material_expressions(shimmer, "", out, "B")
    mel.connect_material_property(out, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(PATH)
    unreal.log("THALASSA: created " + PATH)
