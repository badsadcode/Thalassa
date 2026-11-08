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
sun.light_component.set_editor_property("light_source_angle", 30.0)  # Light from above is diffuse down here: very soft shadows.

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

# ---- Rock formations, placed by rules rather than at random. See docs/SEABED.md.
import math
import random

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def ground(x, y):
    """The seabed's height at a world X and Y, in centimetres, found by a trace straight down."""
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 0.0), unreal.Vector(x, y, -100000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    values = hit.to_tuple()
    return values[5].z if values[0] else None


def lowest(x, y, radius):
    """The lowest ground under a footprint, so a rock sunk to it never shows a gap on a slope."""
    heights = [ground(x + dx, y + dy) for dx, dy in ((0, 0), (radius, 0), (-radius, 0), (0, radius), (0, -radius))]
    heights = [h for h in heights if h is not None]
    return min(heights) if heights else None


def place(mesh_name, x, y, z, yaw, scale, pitch=0.0, roll=0.0, label=None):
    mesh = unreal.load_asset("/Game/World/Formations/" + mesh_name)
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.static_mesh_component.set_mobility(unreal.ComponentMobility.STATIC)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    actor.set_actor_label(label or mesh_name)
    actor.set_folder_path("Formations")
    return actor


rng = random.Random(20261108)
placed = 0

# The bent monolith: alone on the flat basin floor, 650 m astern of the start and clear of every test route.
MONOLITH = (-50000.0, -42000.0)
base = lowest(MONOLITH[0], MONOLITH[1], 2500.0)
if base is not None:
    place("SM_Monolith_Bent", MONOLITH[0], MONOLITH[1], base - 300.0, 35.0, 1.0, label="Monolith_Bent")
    placed += 1
    # Its broken-off top lies at its foot, on the side it leans toward: a few huge plates.
    for k in range(5):
        angle = math.radians(35.0 + rng.uniform(-40.0, 40.0))
        distance = rng.uniform(4000.0, 9000.0)
        fx, fy = MONOLITH[0] + math.cos(angle) * distance, MONOLITH[1] + math.sin(angle) * distance
        fz = lowest(fx, fy, 800.0)
        if fz is not None:
            place("SM_SlabFragment_%02d" % rng.randint(1, 5), fx, fy, fz - 80.0, rng.uniform(0, 360), rng.uniform(3.0, 5.5),
                  rng.uniform(-20, 20), rng.uniform(-20, 20), label="Monolith_Fallen_%d" % k)
            placed += 1

# Slab towers: only on moderate slopes in the south-west, spaced apart, each with broken plates at its feet.
towers = []
candidates = [(gx, gy) for gx in range(-2200, -900, 50) for gy in range(-2200, -800, 50)]  # Clear of the map's rim.
rng.shuffle(candidates)
for gx, gy in candidates:
    x, y = gx * 100.0 + rng.uniform(-2000, 2000), gy * 100.0 + rng.uniform(-2000, 2000)
    h0, hx, hy = ground(x, y), ground(x + 1000.0, y), ground(x, y + 1000.0)
    if None in (h0, hx, hy):
        continue
    slope = math.hypot(hx - h0, hy - h0) / 1000.0
    if not 0.12 < slope < 0.6 or rng.random() > 0.35:
        continue
    if any(math.hypot(x - tx, y - ty) < 7000.0 for tx, ty in towers):
        continue
    towers.append((x, y))
    scale = rng.uniform(0.8, 1.6)
    z = lowest(x, y, 900.0 * scale)
    if z is None:
        continue
    place("SM_SlabTower_%02d" % rng.randint(1, 6), x, y, z - 50.0, rng.uniform(0, 360), scale, rng.uniform(-3, 3), rng.uniform(-3, 3))
    placed += 1
    for k in range(rng.randint(5, 11)):
        angle = rng.uniform(0, math.tau)
        distance = rng.uniform(1.2, 3.2) * 900.0 * scale
        fx, fy = x + math.cos(angle) * distance, y + math.sin(angle) * distance
        fz = ground(fx, fy)
        if fz is not None:
            place("SM_SlabFragment_%02d" % rng.randint(1, 5), fx, fy, fz - 20.0, rng.uniform(0, 360), rng.uniform(0.6, 2.2),
                  rng.uniform(-25, 25), rng.uniform(-25, 25))
            placed += 1
    if len(towers) >= 14:
        break
unreal.log("THALASSA: placed %d formation pieces, %d slab towers" % (placed, len(towers)))

if not level.save_current_level():
    raise RuntimeError("could not save " + MAP_PATH)
unreal.log("THALASSA: created " + MAP_PATH)
