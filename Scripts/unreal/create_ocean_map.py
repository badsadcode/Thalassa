"""Creates or rebuilds /Game/Maps/L_Ocean: the open ocean, generated from a seed and streamed. See docs/OCEAN.md.

No baked seabed: an AThalassaOcean actor builds it at runtime around whoever is there. The water's look is
shared with the test map (thalassa_water.py). Formations, rocks and life will come back as placement rules
on the generated terrain once it looks right; for now the open ocean is terrain alone, on purpose.

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript=<this file> -unattended
"""
import os
import sys
import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import thalassa_water  # noqa: E402

MAP_PATH = "/Game/Maps/L_Ocean"
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    if not level.load_level(MAP_PATH):
        raise RuntimeError("could not load " + MAP_PATH)
    for existing in actors.get_all_level_actors():
        actors.destroy_actor(existing)
elif not level.new_level(MAP_PATH):
    raise RuntimeError("could not create " + MAP_PATH)

thalassa_water.add_water(actors)

ocean = actors.spawn_actor_from_class(unreal.ThalassaOcean, unreal.Vector(0, 0, 0))
ocean.set_actor_label("Ocean")

if not level.save_current_level():
    raise RuntimeError("could not save " + MAP_PATH)
unreal.log("THALASSA: created " + MAP_PATH)
