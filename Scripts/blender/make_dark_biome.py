"""Generates the first biome's rocks and life, and exports them as FBX for Scripts/unreal/import_dark_biome.py.

The dark biome: gloomy, with almost no light but what lives there.

  SM_DarkBoulder_01..06   Fractured dark boulders: lumpy, with crack lines from Voronoi cells, 30-50k vertices.
  SM_SeaPen_01..03        Feather-like animals on a stalk, 0.8 to 1.6 m, rooted in soft sediment. They glow faintly.
  SM_TubeWorms_01..03     Clusters of 15 to 40 pale tubes with red plumes, around warm seeps.
  SM_SeaWhip_01..03       Long thin stalks, 2 to 5 m, curving, sometimes branching, with glowing tips.
  SM_GlowBulb_01..03      Small clusters of stalked bulbs whose heads glow: the brightest life here.

Vertex colours carry what the life material needs, not colour:
  red   how far a point sways in the current: 0 at the root, 1 at the tip.
  green how strongly it glows.
  blue  0 for pale tissue, 1 for red plume.
Each object's origin is its root, at ground level. Deterministic: fixed seeds.

Run with: python Scripts/build/blender.py Scripts/blender/make_dark_biome.py -- <output folder>
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Vector, noise

out_dir = sys.argv[sys.argv.index("--") + 1]


def finish(name, bm, colours, smooth=True):
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    mesh.polygons.foreach_set("use_smooth", [smooth] * len(mesh.polygons))
    attribute = mesh.color_attributes.new("Col", "BYTE_COLOR", "POINT")
    for index, colour in enumerate(colours):
        attribute.data[index].color = (*colour, 1.0)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    return obj


def tube(bm, colours, path, radii, sides, sway, glow, plume):
    """A tube along a path of points, with a radius at each point. Colours as described above."""
    rings = []
    for k, point in enumerate(path):
        ahead = path[min(k + 1, len(path) - 1)] - path[max(k - 1, 0)]
        ahead.normalize()
        side = ahead.cross(Vector((0, 0, 1)) if abs(ahead.z) < 0.95 else Vector((1, 0, 0)))
        side.normalize()
        up = side.cross(ahead)
        t = k / (len(path) - 1)
        ring = []
        for s in range(sides):
            a = math.tau * s / sides
            ring.append(bm.verts.new(point + (side * math.cos(a) + up * math.sin(a)) * radii[k]))
            colours.append((sway(t), glow(t), plume(t)))
        rings.append(ring)
    for k in range(len(rings) - 1):
        for s in range(sides):
            a, b = s, (s + 1) % sides
            bm.faces.new((rings[k][a], rings[k][b], rings[k + 1][b], rings[k + 1][a]))
    tip = bm.verts.new(path[-1] + (path[-1] - path[-2]).normalized() * radii[-1] * 0.5)
    colours.append((sway(1.0), glow(1.0), plume(1.0)))
    for s in range(sides):
        bm.faces.new((rings[-1][s], rings[-1][(s + 1) % sides], tip))


def curved_path(rng, height, steps, bend, wobble=0.0):
    lean = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), 0)).normalized() * bend
    seed = rng.uniform(0, 100)
    return [Vector((lean.x * (k / steps) ** 2 + wobble * noise.noise((seed, k * 0.3, 0.0)),
                    lean.y * (k / steps) ** 2 + wobble * noise.noise((seed, k * 0.3, 5.0)),
                    height * k / steps)) for k in range(steps + 1)]


def dark_boulder(index):
    rng = random.Random(9100 + index)
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=6, radius=1.0)
    seed = Vector((rng.uniform(0, 50), rng.uniform(0, 50), rng.uniform(0, 50)))
    stretch = Vector((rng.uniform(0.8, 1.4), rng.uniform(0.7, 1.2), rng.uniform(0.45, 0.8)))
    colours = []
    for v in bm.verts:
        p = v.co.copy()
        q = p * 1.6 + seed
        # Lumps, then crack lines along Voronoi cell edges, then fine grit.
        lumps = noise.fractal(q, 1.0, 2.0, 5, noise_basis="PERLIN_ORIGINAL") * 0.22
        distances, _ = noise.voronoi(p * 2.2 + seed, distance_metric="DISTANCE", exponent=2.5)
        crack = max(0.0, 0.18 - (distances[1] - distances[0])) * 0.9
        grit = noise.noise(p * 14.0 + seed) * 0.012
        r = 1.0 + lumps - crack + grit
        v.co = Vector((p.x * stretch.x, p.y * stretch.y, p.z * stretch.z)) * r
        if v.co.z < -0.25 * stretch.z:
            v.co.z = -0.25 * stretch.z + (v.co.z + 0.25 * stretch.z) * 0.3  # A flatter underside: it rests, it doesn't balance.
        shade = 0.22 + 0.06 * noise.noise(q * 0.7) - crack * 0.4
        colours.append((shade, shade, shade * 1.02))
    for v in bm.verts:
        v.co.z += 0.25 * stretch.z  # Origin at the ground line.
    return finish("SM_DarkBoulder_%02d" % index, bm, colours)


def sea_pen(index):
    rng = random.Random(9200 + index)
    bm = bmesh.new()
    colours = []
    height = rng.uniform(0.8, 1.6)
    path = curved_path(rng, height, 24, height * 0.12)
    radii = [0.018 * (1.0 - 0.6 * k / 24) for k in range(25)]
    sway = lambda t: t ** 1.5
    tube(bm, colours, path, radii, 8, sway, lambda t: 0.25 * t, lambda t: 0.0)
    # The feather: pairs of thin leaflets along the upper part, angled up, like a quill.
    for k in range(10, 24):
        t = k / 24
        base = path[k]
        length = 0.13 * math.sin(math.pi * (k - 10) / 14) + 0.02
        for side in (-1, 1):
            out = Vector((side * 0.7, 0.0, 0.7)).normalized() * length
            width = Vector((0.0, 0.012, 0.0))
            corners = [base - width, base + width, base + out + width * 0.4, base + out - width * 0.4]
            vs = [bm.verts.new(c) for c in corners]
            for c in corners:
                colours.append((sway(t) + 0.15, 0.55, 0.0))
            bm.faces.new(vs)
    return finish("SM_SeaPen_%02d" % index, bm, colours)


def tube_worms(index):
    rng = random.Random(9300 + index)
    bm = bmesh.new()
    colours = []
    for k in range(rng.randint(15, 40)):
        angle, distance = rng.uniform(0, math.tau), abs(rng.gauss(0.0, 0.18))
        root = Vector((math.cos(angle) * distance, math.sin(angle) * distance, -0.05))
        height = rng.uniform(0.5, 2.0) * (1.0 - distance)
        path = [root + p for p in curved_path(rng, height, 14, height * 0.15, 0.02)]
        r = rng.uniform(0.018, 0.035)
        radii = [r] * 15
        tube(bm, colours, path, radii, 7, lambda t: 0.12 * t, lambda t: 0.0, lambda t: 0.0)
        # The plume: a red feathery flare at the top.
        tip = path[-1]
        plume_path = [tip + Vector((0, 0, 0.02 * j)) for j in range(6)]
        tube(bm, colours, plume_path, [r * (1.0 + 1.6 * j / 5) for j in range(6)], 7,
             lambda t: 0.35 + 0.3 * t, lambda t: 0.0, lambda t: 1.0)
    return finish("SM_TubeWorms_%02d" % index, bm, colours)


def sea_whip(index):
    rng = random.Random(9400 + index)
    bm = bmesh.new()
    colours = []
    height = rng.uniform(2.0, 5.0)
    steps = 40
    path = curved_path(rng, height, steps, height * 0.35, 0.08)
    tube(bm, colours, path, [0.03 * (1.0 - 0.75 * k / steps) for k in range(steps + 1)], 6,
         lambda t: t ** 1.3, lambda t: max(0.0, (t - 0.85) / 0.15), lambda t: 0.0)
    for branch in range(rng.randint(0, 2)):
        at = rng.randint(int(steps * 0.3), int(steps * 0.6))
        sub = [path[at] + p for p in curved_path(rng, height * 0.45, 20, height * 0.3, 0.05)]
        tube(bm, colours, sub, [0.018 * (1.0 - 0.7 * k / 20) for k in range(21)], 6,
             lambda t, s=at / steps: s + (1 - s) * t ** 1.3, lambda t: max(0.0, (t - 0.8) / 0.2), lambda t: 0.0)
    return finish("SM_SeaWhip_%02d" % index, bm, colours)


def glow_bulbs(index):
    rng = random.Random(9500 + index)
    bm = bmesh.new()
    colours = []
    for k in range(rng.randint(4, 11)):
        angle, distance = rng.uniform(0, math.tau), rng.uniform(0.0, 0.25)
        root = Vector((math.cos(angle) * distance, math.sin(angle) * distance, -0.03))
        height = rng.uniform(0.08, 0.35)
        path = [root + p for p in curved_path(rng, height, 8, height * 0.3)]
        tube(bm, colours, path, [0.012] * 9, 6, lambda t: 0.3 * t, lambda t: 0.0, lambda t: 0.0)
        head = bmesh.ops.create_uvsphere(bm, u_segments=10, v_segments=8, radius=rng.uniform(0.03, 0.06))
        for v in head["verts"]:
            v.co = Vector((v.co.x, v.co.y, v.co.z * 1.3)) + path[-1]
            colours.append((0.35, 1.0, 0.0))
    return finish("SM_GlowBulb_%02d" % index, bm, colours)


bpy.ops.wm.read_factory_settings(use_empty=True)
for i in range(1, 7):
    dark_boulder(i)
for i in range(1, 4):
    sea_pen(i)
    tube_worms(i)
    sea_whip(i)
    glow_bulbs(i)

for obj in bpy.context.scene.collection.objects:
    zs = [v.co.z for v in obj.data.vertices]
    print("THALASSA: %s, %d vertices, %.2f m to %.2f m" % (obj.name, len(obj.data.vertices), min(zs), max(zs)))

os.makedirs(out_dir, exist_ok=True)
for group, prefix in (("DarkBoulders", "SM_DarkBoulder"), ("Life", "SM_")):
    for obj in bpy.context.scene.collection.objects:
        obj.select_set(obj.name.startswith(prefix) and (group == "DarkBoulders" or not obj.name.startswith("SM_DarkBoulder")))
    path = os.path.join(out_dir, group + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="OFF", colors_type="LINEAR",
                             add_leaf_bones=False, bake_anim=False)
    print("THALASSA: exported " + path)
