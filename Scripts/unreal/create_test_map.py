"""Creates or rebuilds /Game/TestMaps/L_ShipMotion: light, fog and the generated seabed.

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript=<this file> -unattended
"""
import unreal

MAP_PATH = "/Game/TestMaps/L_ShipMotion"

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

# ---- The water. Values here are the look: tune them by capturing frames (docs/ATMOSPHERE.md).
LIGHT_FROM_ABOVE = 0.35                                    # lux: what filters down to 200 m. Bright sun is ~100,000.
LIGHT_COLOUR = unreal.LinearColor(0.35, 0.75, 1.0, 1.0)    # Red is gone first underwater, then green.
WATER_DENSITY = 0.07                                       # Fog density: how quickly things fade into the murk.
WATER_COLOUR = unreal.LinearColor(0.0012, 0.008, 0.012, 1.0)  # What the murk itself glows, lit from above.
SCATTER_ALBEDO = unreal.Color(110, 150, 175, 255)         # How the water tints light scattered in it.
EXPOSURE_BIAS = 2.5                                        # Manual exposure, so the darkness stays dark.

# Light from above: cold, dim, straight down, with beams through the murk.
sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 0), unreal.Rotator(0, -80, 20))
sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sun.light_component.set_intensity(LIGHT_FROM_ABOVE)
sun.light_component.set_light_color(LIGHT_COLOUR)
sun.light_component.set_editor_property("volumetric_scattering_intensity", 1.0)

# Ambient light: only from above. Below is the dark.
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 0))
sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sky.light_component.set_intensity(0.25)
sky.light_component.set_editor_property("lower_hemisphere_is_black", True)
sky.light_component.set_editor_property("lower_hemisphere_color", unreal.LinearColor(0, 0, 0, 1))

# The water itself: even murk at every depth, and volumetric scattering so lamps cast beams.
fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
f = fog.component
f.set_editor_property("fog_density", WATER_DENSITY)
f.set_editor_property("fog_height_falloff", 0.0001)
f.set_editor_property("fog_max_opacity", 1.0)
f.set_editor_property("fog_inscattering_luminance", WATER_COLOUR)
f.set_editor_property("enable_volumetric_fog", True)
f.set_editor_property("volumetric_fog_scattering_distribution", 0.7)  # Forward scattering: beams glow toward you.
f.set_editor_property("volumetric_fog_albedo", SCATTER_ALBEDO)
f.set_editor_property("volumetric_fog_extinction_scale", 1.0)
f.set_editor_property("volumetric_fog_distance", 12000.0)

# The camera: fixed exposure, a cold grade, a dark edge and a little grain.
post = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
post.set_editor_property("unbound", True)
pp = post.get_editor_property("settings")
for name, value in [
        ("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL),
        ("auto_exposure_bias", EXPOSURE_BIAS),
        ("auto_exposure_apply_physical_camera_exposure", False),
        ("color_saturation", unreal.Vector4(0.85, 0.95, 1.0, 0.9)),
        ("color_gain", unreal.Vector4(0.92, 1.0, 1.04, 1.0)),
        ("vignette_intensity", 0.55),
        ("film_grain_intensity", 0.12),
        ("bloom_intensity", 0.8),
        ("scene_fringe_intensity", 0.6)]:
    pp.set_editor_property("override_" + name, True)
    pp.set_editor_property(name, value)
post.set_editor_property("settings", pp)

# The seabed: generated terrain, made by Scripts/blender/make_seabed.py and imported by Scripts/unreal/import_seabed.py.
seabed_mesh = unreal.load_asset("/Game/World/Seabed/SM_Seabed")
if not seabed_mesh:
    raise RuntimeError("no /Game/World/Seabed/SM_Seabed: run Scripts/unreal/import_seabed.py first")
seabed = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
seabed.static_mesh_component.set_static_mesh(seabed_mesh)
seabed.static_mesh_component.set_mobility(unreal.ComponentMobility.STATIC)
seabed.set_actor_label("Seabed")

if not level.save_current_level():
    raise RuntimeError("could not save " + MAP_PATH)
unreal.log("THALASSA: created " + MAP_PATH)
