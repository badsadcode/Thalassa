"""Generates the test seabed as a mesh and exports it as FBX, for Scripts/unreal/import_seabed.py.

6 x 6 km around the ship's start, one vertex every 10 m. Deterministic: Blender's noise functions take no
random seed, so the same script always makes the same seabed, and the sonar always charts the same thing.

  - A basin about 500 m down under the start, so the ship starts with room below it.
  - Ridges further out, climbing to about 280 m.
  - A winding canyon to starboard, down to about 660 m.
  - A seamount ahead and to port, its peak about 120 m below the surface.
  - A gentle rise at the edges, so the map does not end in a cliff.

Vertex colours hold sand on the flats and rock on the slopes, for the seabed material.

Run with: python Scripts/build/blender.py Scripts/blender/make_seabed.py -- <output.fbx>
"""
import math
import os
import sys

import bpy
from mathutils import noise

out_path = sys.argv[sys.argv.index("--") + 1]

SIZE_M = 6000.0
CELLS = 600
STEP = SIZE_M / CELLS
HALF = SIZE_M / 2.0


def smoothstep(e0, e1, x):
    t = min(max((x - e0) / (e1 - e0), 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def height(x, y):
    """Seabed height in metres at Unreal X (forward) and Y (starboard), 0 being the surface."""
    d = math.hypot(x, y)
    h = -500.0
    # Gentle rolling everywhere.
    h += noise.fractal((x / 900.0, y / 900.0, 0.3), 1.0, 2.0, 6, noise_basis="PERLIN_ORIGINAL") * 22.0
    # Ridges, kept away from the start.
    ridge = noise.ridged_multi_fractal((x / 1500.0 + 7.1, y / 1500.0 - 3.3, 0.0), 1.0, 2.0, 5, 1.0, 2.0, noise_basis="PERLIN_ORIGINAL")
    h += max(ridge - 0.6, 0.0) * 160.0 * smoothstep(700.0, 2000.0, d)
    # A winding canyon to starboard.
    centre = 1100.0 + 280.0 * math.sin(x / 650.0) + 120.0 * math.sin(x / 260.0 + 1.3)
    across = abs(y - centre)
    h -= 160.0 * (1.0 - smoothstep(60.0, 260.0, across))
    # A seamount ahead and to port.
    peak = math.hypot(x - 2100.0, y + 1700.0)
    h += 380.0 * math.exp(-(peak / 430.0) ** 2) * (1.0 + 0.15 * noise.noise((x / 120.0, y / 120.0, 2.0)))
    # Rising edges.
    edge = max(abs(x), abs(y))
    h += 140.0 * smoothstep(2300.0, 3000.0, edge)
    # Where ridges, seamount and edge pile up, flatten the tops: nothing comes within about 100 m of the surface.
    if h > -150.0:
        h = -150.0 + (h + 150.0) * 0.25
    return h


n = CELLS + 1
heights = [[height(-HALF + i * STEP, -HALF + j * STEP) for j in range(n)] for i in range(n)]

verts = []
colours = []
SAND = (0.50, 0.47, 0.40)
ROCK = (0.16, 0.17, 0.18)
for i in range(n):
    for j in range(n):
        x = -HALF + i * STEP
        y = -HALF + j * STEP
        z = heights[i][j]
        # Blender is right-handed: Unreal's Y is mirrored. Metres either way.
        verts.append((x, -y, z))
        dzdx = (heights[min(i + 1, n - 1)][j] - heights[max(i - 1, 0)][j]) / (2.0 * STEP)
        dzdy = (heights[i][min(j + 1, n - 1)] - heights[i][max(j - 1, 0)]) / (2.0 * STEP)
        steep = smoothstep(0.25, 0.7, math.hypot(dzdx, dzdy))
        # Darker the deeper, so the canyon floor reads as deep.
        depth = smoothstep(-500.0, -660.0, z) * 0.35
        c = [SAND[k] * (1.0 - steep) + ROCK[k] * steep for k in range(3)]
        colours.append((c[0] * (1.0 - depth), c[1] * (1.0 - depth), c[2] * (1.0 - depth), 1.0))

faces = []
for i in range(CELLS):
    for j in range(CELLS):
        a = i * n + j
        # Wound so the faces point up after the Y mirror.
        faces.append((a, a + 1, a + n + 1, a + n))

bpy.ops.wm.read_factory_settings(use_empty=True)
mesh = bpy.data.meshes.new("SM_Seabed")
mesh.from_pydata(verts, [], faces)
mesh.update()
up = sum(p.normal.z for p in mesh.polygons[:1000])
if up < 0:
    mesh.flip_normals()
mesh.polygons.foreach_set("use_smooth", [True] * len(mesh.polygons))  # Rolling ground, not facets.
attribute = mesh.color_attributes.new("Col", "BYTE_COLOR", "POINT")
for index, colour in enumerate(colours):
    attribute.data[index].color = colour

obj = bpy.data.objects.new("SM_Seabed", mesh)
bpy.context.scene.collection.objects.link(obj)

lowest = min(min(row) for row in heights)
highest = max(max(row) for row in heights)
print("THALASSA: seabed from %.0f m to %.0f m, start at %.0f m" % (lowest, highest, heights[n // 2][n // 2]))

os.makedirs(os.path.dirname(out_path), exist_ok=True)
bpy.ops.export_scene.fbx(filepath=out_path, use_selection=False, object_types={"MESH"}, apply_unit_scale=True,
                         apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="OFF", colors_type="SRGB",
                         add_leaf_bones=False, bake_anim=False)
print("THALASSA: exported " + out_path)
