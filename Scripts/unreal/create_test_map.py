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
rock_actors = []  # Everything placed so far: ground traces ignore it, so nothing lands on a ledge.


def ground(x, y):
    """The seabed's height at a world X and Y, in centimetres, found by a trace straight down."""
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 0.0), unreal.Vector(x, y, -100000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, rock_actors, unreal.DrawDebugTrace.NONE, True)
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
    rock_actors.append(actor)
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

# ---- Scanned CC0 rocks (Poly Haven, Scripts/unreal/import_rock_models.py), at many sizes: a scan has no scale.
ROCK_MESHES = {
    # name: (asset path, real largest dimension in cm)
    "moon_01": ("/Game/World/Rocks/moon_rock_01/moon_rock_01/StaticMeshes/moon_rock_01_LOD0", 21.0),
    "moon_03": ("/Game/World/Rocks/moon_rock_03/moon_rock_03/StaticMeshes/moon_rock_03", 11.0),
    "moon_05": ("/Game/World/Rocks/moon_rock_05/moon_rock_05/StaticMeshes/moon_rock_05", 8.0),
    "rock_07": ("/Game/World/Rocks/rock_07/rock_07/StaticMeshes/rock_07", 32.0),
    "rock_09": ("/Game/World/Rocks/rock_09/rock_09/StaticMeshes/rock_09", 14.0),
    "boulder_03": ("/Game/World/Rocks/namaqualand_boulder_03/namaqualand_boulder_03/StaticMeshes/namaqualand_boulder_03", 307.0),
    "boulder_05": ("/Game/World/Rocks/namaqualand_boulder_05/namaqualand_boulder_05/StaticMeshes/namaqualand_boulder_05", 136.0),
    "coast_05": ("/Game/World/Rocks/coast_rocks_05/coast_rocks_05/StaticMeshes/coast_rocks_05", 404.0),
}
SMALL_SCANS = ["moon_01", "moon_03", "moon_05", "rock_07", "rock_09"]
scans = 0


def scan(key, x, y, size_cm, label):
    """Places a scanned rock scaled so its largest dimension is size_cm, sunk into the ground."""
    global scans
    path, real = ROCK_MESHES[key]
    mesh = unreal.load_asset(path)
    if not mesh:
        raise RuntimeError("THALASSA: no rock mesh at " + path)
    z = lowest(x, y, size_cm * 0.5)
    if z is None:
        return
    scale = size_cm / real
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z - 0.12 * size_cm),
                                          unreal.Rotator(roll=rng.uniform(-12, 12), pitch=rng.uniform(-12, 12), yaw=rng.uniform(0, 360)))
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.static_mesh_component.set_mobility(unreal.ComponentMobility.STATIC)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale * rng.uniform(0.8, 1.1)))
    actor.set_actor_label(label)
    actor.set_folder_path("Formations/Scanned")
    rock_actors.append(actor)
    scans += 1


# Boulders around each slab tower: fallen and weathered pieces, 1 to 5 m.
for index, (tx, ty) in enumerate(towers):
    for k in range(rng.randint(4, 8)):
        angle, distance = rng.uniform(0, math.tau), rng.uniform(1200.0, 3500.0)
        scan(rng.choice(SMALL_SCANS), tx + math.cos(angle) * distance, ty + math.sin(angle) * distance, rng.uniform(100.0, 500.0), "TowerBoulder_%d_%d" % (index, k))

# The monolith's foot: a heap of rubble on its leaning side, and big boulders around it.
for k in range(12):
    angle = math.radians(35.0 + rng.uniform(-70.0, 70.0))
    distance = rng.uniform(3000.0, 7000.0)
    scan(rng.choice(SMALL_SCANS + ["boulder_03"]), MONOLITH[0] + math.cos(angle) * distance, MONOLITH[1] + math.sin(angle) * distance,
         rng.uniform(200.0, 900.0), "MonolithRubble_%d" % k)
for k in range(2):
    angle = math.radians(35.0 + rng.uniform(-30.0, 30.0))
    scan("coast_05", MONOLITH[0] + math.cos(angle) * 4500.0, MONOLITH[1] + math.sin(angle) * 4500.0, rng.uniform(1800.0, 2800.0), "MonolithHeap_%d" % k)

# Medium landmarks: the coastal rock mass, 16 to 32 m across, on a few south-west slopes.
for index, (tx, ty) in enumerate(towers[:5]):
    angle = rng.uniform(0, math.tau)
    scan("coast_05", tx + math.cos(angle) * 9000.0, ty + math.sin(angle) * 9000.0, rng.uniform(1600.0, 3200.0), "RockMass_%d" % index)

# The open flats: mostly empty on purpose, with the occasional lone boulder, 2 to 9 m.
lone = 0
for attempt in range(400):
    x, y = rng.uniform(-260000.0, 260000.0), rng.uniform(-260000.0, 260000.0)
    h0, hx, hy = ground(x, y), ground(x + 1000.0, y), ground(x, y + 1000.0)
    if None in (h0, hx, hy) or math.hypot(hx - h0, hy - h0) / 1000.0 > 0.08:
        continue
    scan(rng.choice(["boulder_03", "boulder_05", "moon_01", "rock_07"]), x, y, rng.uniform(200.0, 900.0), "LoneBoulder_%d" % lone)
    lone += 1
    if lone >= 45:
        break
unreal.log("THALASSA: placed %d scanned rocks, %d of them lone boulders on the flats" % (scans, lone))

# ---- The dark biome: gloomy, nearly lightless, lit only by what lives there. See docs/SEABED.md.
BIOME = (190000.0, 200000.0)   # North-east, clear of the start, the canyon's middle and every test route.
BIOME_RADIUS = 60000.0
life = {}  # mesh name: transforms, for one instanced scatter actor per mesh


def in_biome():
    angle, distance = rng.uniform(0, math.tau), BIOME_RADIUS * math.sqrt(rng.random())
    return BIOME[0] + math.cos(angle) * distance, BIOME[1] + math.sin(angle) * distance


def grow(kind, x, y, scale_low, scale_high, tilt=8.0):
    """One living thing, rooted in the ground where it stands."""
    z = ground(x, y)
    if z is None:
        return
    scale = rng.uniform(scale_low, scale_high)
    name = "SM_%s_%02d" % (kind, rng.randint(1, 3))
    life.setdefault(name, []).append(unreal.Transform(unreal.Vector(x, y, z - 2.0),
                                                      unreal.Rotator(roll=rng.uniform(-tilt, tilt), pitch=rng.uniform(-tilt, tilt), yaw=rng.uniform(0, 360)),
                                                      unreal.Vector(scale, scale, scale)))


def slope_at(x, y):
    h0, hx, hy = ground(x, y), ground(x + 500.0, y), ground(x, y + 500.0)
    return None if None in (h0, hx, hy) else math.hypot(hx - h0, hy - h0) / 500.0


# Rock clusters: a few big fractured boulders with smaller ones around them.
dark_rocks = 0
clusters = [in_biome() for _ in range(14)]
for cx, cy in clusters:
    for k in range(rng.randint(2, 6)):
        size = rng.uniform(3.0, 14.0) if k == 0 else rng.uniform(1.5, 6.0)
        angle, distance = rng.uniform(0, math.tau), (0.0 if k == 0 else rng.uniform(4.0, 14.0) * 100.0)
        x, y = cx + math.cos(angle) * distance, cy + math.sin(angle) * distance
        z = lowest(x, y, size * 50.0)
        if z is None:
            continue
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z - size * 15.0),
                                              unreal.Rotator(roll=rng.uniform(-10, 10), pitch=rng.uniform(-10, 10), yaw=rng.uniform(0, 360)))
        actor.static_mesh_component.set_static_mesh(unreal.load_asset("/Game/World/DarkBiome/SM_DarkBoulder_%02d" % rng.randint(1, 6)))
        actor.static_mesh_component.set_mobility(unreal.ComponentMobility.STATIC)
        actor.set_actor_scale3d(unreal.Vector(size * 0.5, size * 0.5, size * 0.5))  # The meshes are about 2 m across.
        actor.set_actor_label("DarkBoulder_%d" % dark_rocks)
        actor.set_folder_path("DarkBiome")
        rock_actors.append(actor)
        dark_rocks += 1
        # Life clings to rock: whips and glowing bulbs at the boulders' feet.
        for j in range(rng.randint(2, 6)):
            a2, d2 = rng.uniform(0, math.tau), size * rng.uniform(55.0, 90.0)
            grow("GlowBulb" if rng.random() < 0.5 else "SeaWhip", x + math.cos(a2) * d2, y + math.sin(a2) * d2, 0.8, 1.5)

# Warm seeps: tube worms crowd around them, with glowing bulbs in the gaps. The brightest places here.
seeps = [in_biome() for _ in range(4)]
for sx, sy in seeps:
    for k in range(rng.randint(8, 14)):
        angle, distance = rng.uniform(0, math.tau), abs(rng.gauss(0.0, 600.0))
        grow("TubeWorms", sx + math.cos(angle) * distance, sy + math.sin(angle) * distance, 0.8, 1.6, 5.0)
    for k in range(rng.randint(25, 45)):
        angle, distance = rng.uniform(0, math.tau), abs(rng.gauss(0.0, 1500.0))
        grow("GlowBulb", sx + math.cos(angle) * distance, sy + math.sin(angle) * distance, 0.8, 2.0)

# Sea pens: patches on soft, flat ground. Sea whips: scattered where the ground slopes.
for patch in range(30):
    px, py = in_biome()
    slope = slope_at(px, py)
    if slope is None or slope > 0.15:
        continue
    for k in range(rng.randint(15, 40)):
        angle, distance = rng.uniform(0, math.tau), BIOME_RADIUS * 0.02 * math.sqrt(rng.random())
        grow("SeaPen", px + math.cos(angle) * distance, py + math.sin(angle) * distance, 0.7, 1.3)
for k in range(400):
    x, y = in_biome()
    slope = slope_at(x, y)
    if slope is not None and 0.1 < slope < 0.7:
        grow("SeaWhip", x, y, 0.7, 1.4)

total_life = 0
for name, transforms in sorted(life.items()):
    scatter = actors.spawn_actor_from_class(unreal.ThalassaScatter, unreal.Vector(BIOME[0], BIOME[1], -50000.0))
    scatter.set_scatter_mesh(unreal.load_asset("/Game/World/DarkBiome/" + name), 15000.0)  # Not drawn beyond 150 m: the murk hides it anyway.
    scatter.add_world_instances(transforms)
    scatter.set_actor_label("Life_" + name)
    scatter.set_folder_path("DarkBiome")
    total_life += len(transforms)

# The biome's mood: colder and darker than the open seabed, blending in over 200 m.
gloom = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(BIOME[0], BIOME[1], -40000.0))
gloom.set_actor_scale3d(unreal.Vector(BIOME_RADIUS / 100.0, BIOME_RADIUS / 100.0, 400.0))  # The default brush is 200 cm across.
gloom.set_editor_property("blend_radius", 20000.0)
gloom.set_editor_property("priority", 1.0)
gs = gloom.get_editor_property("settings")
for name, value in [("color_gain", unreal.Vector4(0.55, 0.62, 0.72, 0.8)), ("color_saturation", unreal.Vector4(0.8, 0.85, 0.95, 0.8)),
                    ("vignette_intensity", 0.75)]:
    gs.set_editor_property("override_" + name, True)
    gs.set_editor_property(name, value)
gloom.set_editor_property("settings", gs)
gloom.set_actor_label("DarkBiome_Gloom")
gloom.set_folder_path("DarkBiome")
unreal.log("THALASSA: dark biome: %d boulders, %d living things in %d kinds, %d seeps" % (dark_rocks, total_life, len(life), len(seeps)))

if not level.save_current_level():
    raise RuntimeError("could not save " + MAP_PATH)
unreal.log("THALASSA: created " + MAP_PATH)
