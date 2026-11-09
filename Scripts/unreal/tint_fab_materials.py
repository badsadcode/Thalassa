"""Gives the Fab Marketplace creatures' materials a colour the game can set: the vector parameter ThalassaTint
(white by default, so nothing changes until the game sets it), multiplying each material's base colour.

The game gives each school its own tint from it (UThalassaOceanFauna::DrawAnimated), so the same animal comes in
several looks. It edits the creatures' base materials in Content/<creature>/Materials (instances inherit the
parameter). Safe to run again: a material that already has ThalassaTint is left alone. To undo it, add the
creature to the project from Fab again.

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript=<this file> -unattended -nullrhi
"""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
PARAM = "ThalassaTint"

# The same list as the animated kinds in ThalassaOceanFauna.cpp.
MESHES = ["/Game/Shark_Megalodon/Base_mesh/SK_Shark_Megalodon", "/Game/SharkGoblin/Base_mesh/SK_SharkGoblin",
          "/Game/HammerheadShark/BaseMesh/SK_HammerheadShark", "/Game/BlueWhaleAndWhaleCalf/BaseMesh/BlueWhale/SK_blue_whale",
          "/Game/BlueWhaleAndWhaleCalf/BaseMesh/BlueWhaleCalf/SK_blue_whale_calf", "/Game/DolphinAndRemora/BaseMesh/Dolphin/SK_Dolphin",
          "/Game/SeaTurtle/BaseMesh/SK_SeaTurtle", "/Game/Stingray/Base_mesh/SK_Stingray", "/Game/MorayEel/Base_mesh/SK_MorayEel",
          "/Game/AnglerFish/BaseMesh/SK_AnglerFish", "/Game/MarlinSailfish/Base_mesh/SK_MarlinSailfish"]

done, skipped, failed = 0, 0, 0
seen = set()
for path in MESHES:
    mesh = unreal.load_asset(path)
    if not mesh:
        unreal.log_warning("THALASSA: no mesh at " + path)
        continue
    for slot in mesh.get_editor_property("materials"):
        material = slot.get_editor_property("material_interface")
        base = material.get_base_material() if material else None
        if not base or base.get_path_name() in seen:
            continue
        seen.add(base.get_path_name())
        if PARAM in [str(n) for n in mel.get_vector_parameter_names(base)]:
            skipped += 1
            continue
        source = mel.get_material_property_input_node(base, unreal.MaterialProperty.MP_BASE_COLOR)
        if not source:
            unreal.log_warning("THALASSA: %s has nothing in its base colour; left alone" % base.get_path_name())
            failed += 1
            continue
        output = mel.get_material_property_input_node_output_name(base, unreal.MaterialProperty.MP_BASE_COLOR)
        tint = mel.create_material_expression(base, unreal.MaterialExpressionVectorParameter, -500, -300)
        tint.set_editor_property("parameter_name", PARAM)
        tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
        multiply = mel.create_material_expression(base, unreal.MaterialExpressionMultiply, -250, -300)
        mel.connect_material_expressions(source, output, multiply, "A")
        mel.connect_material_expressions(tint, "", multiply, "B")
        mel.connect_material_property(multiply, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.recompile_material(base)
        eal.save_asset(base.get_path_name())
        unreal.log("THALASSA: tint added to " + base.get_path_name())
        done += 1
unreal.log("THALASSA: Fab creature materials: %d tinted, %d already had it, %d left alone" % (done, skipped, failed))
