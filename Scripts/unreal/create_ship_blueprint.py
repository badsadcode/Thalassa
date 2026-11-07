"""Creates /Game/Ship/BP_Thalassa, the ship Blueprint the art is built in, if it does not exist yet.

The ship's behaviour stays in C++ (AThalassaVessel). The Blueprint is where meshes, rooms and lights are
placed by hand. It is never overwritten: run this again and an existing Blueprint is left alone.

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript=<this file> -unattended
"""
import unreal

PACKAGE_PATH = "/Game/Ship"
ASSET_NAME = "BP_Thalassa"
ASSET_PATH = PACKAGE_PATH + "/" + ASSET_NAME

if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
    unreal.log("THALASSA: " + ASSET_PATH + " already exists; left as it is")
else:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.ThalassaVessel)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(ASSET_NAME, PACKAGE_PATH, unreal.Blueprint, factory)
    if not blueprint:
        raise RuntimeError("could not create " + ASSET_PATH)
    if not unreal.EditorAssetLibrary.save_asset(ASSET_PATH):
        raise RuntimeError("could not save " + ASSET_PATH)
    unreal.log("THALASSA: created " + ASSET_PATH)
