"""Creates or rebuilds /Game/TestMaps/L_ShipMotion: light, fog and a seabed to judge ship motion against.

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript=<this file> -unattended
"""
import unreal

MAP_PATH = "/Game/TestMaps/L_ShipMotion"
SEABED_Z = -50000.0
SPACING = 10000.0
GRID_RADIUS = 6

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Rebuild in place if the map exists: it is the editor's startup map, so it cannot simply be deleted.
if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    if not level.load_level(MAP_PATH):
        raise RuntimeError("could not load " + MAP_PATH)
    for existing in actors.get_all_level_actors():
        actors.destroy_actor(existing)
elif not level.new_level(MAP_PATH):
    raise RuntimeError("could not create " + MAP_PATH)

sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 0), unreal.Rotator(0, -60, 30))
sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sun.light_component.set_intensity(4.0)

sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 0))
sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sky.light_component.set_intensity(1.5)

fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, -30000))
fog.component.set_editor_property("fog_density", 0.002)
fog.component.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.02, 0.18, 0.28, 1.0))

cube = unreal.load_asset("/Engine/BasicShapes/Cube")

# The seabed itself: a flat floor under the landmarks, for the sonar to find.
seabed = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, SEABED_Z - 500.0))
seabed.static_mesh_component.set_static_mesh(cube)
seabed.set_actor_scale3d(unreal.Vector(2000.0, 2000.0, 10.0))
seabed.set_actor_label("Seabed")
for ix in range(-GRID_RADIUS, GRID_RADIUS + 1):
    for iy in range(-GRID_RADIUS, GRID_RADIUS + 1):
        # A fixed, repeating height pattern: static landmarks, so ship motion is easy to read.
        height = 2000.0 + ((ix * 7 + iy * 13) % 5) * 3000.0
        location = unreal.Vector(ix * SPACING, iy * SPACING, SEABED_Z + height * 0.5)
        pillar = actors.spawn_actor_from_class(unreal.StaticMeshActor, location)
        pillar.static_mesh_component.set_static_mesh(cube)
        pillar.set_actor_scale3d(unreal.Vector(30.0, 30.0, height / 100.0))
        pillar.set_actor_label("Landmark_{}_{}".format(ix, iy))

if not level.save_current_level():
    raise RuntimeError("could not save " + MAP_PATH)
unreal.log("THALASSA: created " + MAP_PATH)
