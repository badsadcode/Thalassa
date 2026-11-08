"""Generates the first rock formation family and the bent monolith, and exports them as FBX.

  SM_SlabTower_01..06   Irregular rock plates stacked into towers, 15 to 50 m tall, with overhangs.
  SM_SlabFragment_01..05  Broken plates, 2 to 9 m across, for the debris at the towers' feet.
  SM_Monolith_Bent      A pillar about 300 m tall, bending as it rises, with eroded layers and mineral streaks.

Each object's origin is the centre of its base, at ground level. Every piece reaches several metres below
its origin, so it can be sunk into sloping ground without showing a gap. Deterministic: a fixed random seed
per piece, so the same script always makes the same rocks. Vertex colours carry the strata and the mineral
staining, for the rock material. See docs/SEABED.md.

Run with: blender --background --factory-startup --python make_formations.py -- <output.fbx>
"""
import math
import os
import random
import sys

import bpy
import bmesh
from mathutils import noise, Vector

out_path = sys.argv[sys.argv.index("--") + 1]

# Rock colours: base, lighter and darker strata, and two kinds of mineral staining.
DARK = (0.10, 0.10, 0.10)
GREY = (0.22, 0.21, 0.20)
BUFF = (0.36, 0.32, 0.26)
RUST = (0.42, 0.24, 0.12)
PALE = (0.55, 0.55, 0.50)


def mix(a, b, t):
    return tuple(a[i] * (1.0 - t) + b[i] * t for i in range(3))


def finish(name, bm, colours, smooth=False):
    """Turns a bmesh into an object with per-vertex colours. Plates are hard-edged; the monolith is smooth."""
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
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


def fbm(p, octaves=4):
    return noise.fractal(p, 1.0, 2.0, octaves, noise_basis="PERLIN_ORIGINAL")


def add_slab(bm, colours, rng, centre, radius, thickness, yaw, tilt, colour, jagged):
    """One rock plate: a finely jagged outline, sides banded by thin layers that have weathered back
    unevenly, and a rough, uneven top. Detail is cheap with Nanite; flat plates read as toys."""
    points = 56
    seed = rng.uniform(0, 100)
    outline = []
    for k in range(points):
        angle = yaw + math.tau * k / points
        # Big lobes, then a broken edge on top of them.
        r = radius * (1.0 + 0.22 * jagged * fbm((math.cos(angle) * 1.3 + seed, math.sin(angle) * 1.3, seed), 3)
                      + 0.06 * jagged * noise.noise((math.cos(angle) * 9 + seed, math.sin(angle) * 9, seed)))
        outline.append((math.cos(angle) * r, math.sin(angle) * r))
    tilt_x, tilt_y = tilt
    bands = max(4, int(thickness / 0.35))  # Thin layers, about 35 cm each.
    rings = []
    for band in range(bands + 1):
        z = thickness * band / bands
        # Each band of the side has worn back by a different amount: thin layers you can see.
        # Alternate layers jut out and wear back, so the edge is ribbed with thin ledges, not a smooth wall.
        inset = (1.0 if band % 2 == 0 else 0.955) - 0.05 * abs(noise.noise((seed, band * 1.7, 3.3))) - (0.04 if band == bands else 0.0)
        ring = []
        for k, (x, y) in enumerate(outline):
            # Vertical cracks cut into the edge, and the surface is never quite even.
            crack = max(0.0, noise.ridged_multi_fractal((x * 0.35 + seed, y * 0.35, 0.0), 1.0, 2.0, 2, 1.0, 2.0, noise_basis="PERLIN_ORIGINAL") - 1.2)
            wobble = 1.0 + 0.03 * noise.noise((x * 0.6 + seed, y * 0.6, z * 3.0)) - 0.06 * crack
            px, py = x * inset * wobble, y * inset * wobble
            pz = z + px * tilt_x + py * tilt_y
            ring.append(bm.verts.new((centre[0] + px, centre[1] + py, centre[2] + pz)))
            shade = 0.7 + 0.3 * noise.noise((px * 0.4 + seed, py * 0.4, band * 0.9))
            base = colour if band % 2 == 0 else mix(colour, DARK, 0.25)
            colours.append(tuple(c * shade for c in (mix(base, DARK, 0.35) if band == 0 else base)))
        rings.append(ring)
    for band in range(bands):
        for k in range(points):
            a, b = k, (k + 1) % points
            bm.faces.new((rings[band][a], rings[band][b], rings[band + 1][b], rings[band + 1][a]))
    # The top: rings of points closing toward the middle, displaced into a rough surface. It starts from its
    # own copy of the rim, so the edge stays crisp under smooth shading.
    previous = []
    for k, v in enumerate(rings[-1]):
        previous.append(bm.verts.new(v.co))
        colours.append(colour)
    steps = 4
    for step in range(1, steps + 1):
        f = 1.0 - step / (steps + 1.0)
        ring = []
        for k, v in enumerate(previous):
            x, y = outline[k][0] * f * 0.93, outline[k][1] * f * 0.93
            bump = 0.18 * thickness * fbm((x * 0.25 + seed, y * 0.25, 7.0), 4) + 0.06 * noise.noise((x * 1.5, y * 1.5, seed))
            z = thickness + x * tilt_x + y * tilt_y + bump
            ring.append(bm.verts.new((centre[0] + x, centre[1] + y, centre[2] + z)))
            colours.append(tuple(c * (0.8 + 0.2 * noise.noise((x * 0.3, y * 0.3, seed + 1))) for c in colour))
        for k in range(points):
            a, b = k, (k + 1) % points
            bm.faces.new((previous[a], previous[b], ring[b], ring[a]))
        previous = ring
    middle = bm.verts.new((centre[0], centre[1], centre[2] + thickness + 0.1 * thickness * fbm((seed, seed, 7.0))))
    colours.append(colour)
    for k in range(points):
        bm.faces.new((previous[k], previous[(k + 1) % points], middle))
    bottom = bm.verts.new((centre[0], centre[1], centre[2]))
    colours.append(DARK)
    for k in range(points):
        bm.faces.new((rings[0][(k + 1) % points], rings[0][k], bottom))


def slab_tower(index):
    rng = random.Random(4100 + index)
    bm = bmesh.new()
    colours = []
    layers = rng.randint(4, 9)
    base_radius = rng.uniform(6.0, 14.0)
    z = -4.0  # The lowest plate starts underground, so the tower sits in the seabed.
    offset = Vector((0.0, 0.0))
    for layer in range(layers):
        # Plates get smaller going up, but now and then one is wider than the one below: an overhang.
        radius = base_radius * (1.0 - 0.07 * layer) * (rng.uniform(1.15, 1.45) if rng.random() < 0.3 else rng.uniform(0.8, 1.05))
        thickness = rng.uniform(1.6, 4.5) * (1.6 if layer == 0 else 1.0)
        offset += Vector((rng.uniform(-0.18, 0.18), rng.uniform(-0.18, 0.18))) * base_radius
        colour = rng.choice((GREY, BUFF, GREY, mix(GREY, RUST, 0.3), mix(BUFF, PALE, 0.3)))
        tilt = (rng.uniform(-0.05, 0.05), rng.uniform(-0.05, 0.05))
        add_slab(bm, colours, rng, (offset.x, offset.y, z), radius, thickness, rng.uniform(0, math.tau), tilt, colour, 1.0)
        z += thickness * rng.uniform(0.92, 1.0)
    return finish("SM_SlabTower_%02d" % index, bm, colours, smooth=True)


def slab_fragment(index):
    rng = random.Random(5200 + index)
    bm = bmesh.new()
    colours = []
    radius = rng.uniform(1.0, 4.5)
    thickness = rng.uniform(0.5, 1.6)
    add_slab(bm, colours, rng, (0.0, 0.0, -0.35 * thickness), radius, thickness, rng.uniform(0, math.tau), (0.0, 0.0),
             rng.choice((GREY, BUFF, mix(GREY, RUST, 0.25))), 1.6)
    return finish("SM_SlabFragment_%02d" % index, bm, colours, smooth=True)


def monolith():
    """About 300 m tall, bending to one side as it rises, its softer layers eroded back."""
    rng = random.Random(777)
    bm = bmesh.new()
    colours = []
    height = 300.0
    rings = 900
    segments = 160
    bend = 55.0
    verts = []
    for k in range(rings + 1):
        t = k / rings
        z = -10.0 + t * (height + 10.0)
        centre = Vector((bend * t * t, 6.0 * math.sin(t * 2.3), z))
        radius = 25.0 + (11.0 - 25.0) * (t ** 0.8)
        radius *= 1.0 + 0.35 * max(0.0, 1.0 - (z + 10.0) / 22.0)  # A flared foot, buried in sediment.
        # Strata of varying thickness. The tilt below makes them dip a few degrees across the pillar.
        warped = z + 9.0 * fbm((0.0, 0.0, z / 60.0), 3)
        layer = (warped / (5.5 + 3.5 * (0.5 + 0.5 * noise.noise((0.0, 3.0, z / 45.0))))) % 1.0
        # Soft layers have worn back further than hard ones, with a sharp lip at the top of each hard layer.
        soft = 0.85 if layer < 0.30 else (1.0 + 0.03 * max(0.0, (layer - 0.85) / 0.15))
        ring = []
        for s in range(segments):
            angle = math.tau * s / segments
            ca, sa = math.cos(angle), math.sin(angle)
            n = fbm((ca * 1.7, sa * 1.7, z / 18.0), 5)
            # Vertical fractures: narrow grooves running a long way up the face.
            crack = noise.ridged_multi_fractal((ca * 3.0, sa * 3.0, z / 90.0), 1.0, 2.0, 3, 1.0, 2.0, noise_basis="PERLIN_ORIGINAL")
            fine = noise.noise((ca * 14, sa * 14, z / 1.2))
            # The strata dip: the same layer sits a little higher on one side of the pillar than the other.
            dip_layer = ((warped + 0.08 * radius * ca + 0.04 * radius * sa) / 7.0) % 1.0
            local_soft = 0.85 if dip_layer < 0.28 else 1.0
            r = radius * (0.5 * soft + 0.5 * local_soft) * (1.0 + 0.13 * n - 0.05 * max(0.0, crack - 1.1) + 0.012 * fine)
            ring.append(bm.verts.new(centre + Vector((math.cos(angle) * r, math.sin(angle) * r, 0.0))))
            # Strata, mineral streaks running down the face, and a darker foot.
            stripe = 0.5 + 0.5 * math.sin(z / 7.5 * math.tau)
            colour = mix(GREY, BUFF, 0.35 * stripe) if soft == 1.0 else mix(GREY, DARK, 0.3)
            streak = noise.noise((math.cos(angle) * 4.0, math.sin(angle) * 4.0, z / 60.0))
            if streak > 0.35:
                colour = mix(colour, RUST, min(1.0, (streak - 0.35) * 3.0))
            elif streak < -0.45:
                colour = mix(colour, PALE, min(1.0, (-0.45 - streak) * 3.0))
            colour = mix(colour, DARK, max(0.0, 1.0 - (z + 10.0) / 30.0) * 0.6)
            colours.append(colour)
        verts.append(ring)
    for k in range(rings):
        for s in range(segments):
            a, b = s, (s + 1) % segments
            bm.faces.new((verts[k][a], verts[k][b], verts[k + 1][b], verts[k + 1][a]))
    # A broken top: an uneven, slightly sunken cap rather than a neat point.
    top = verts[-1]
    centre = sum((v.co for v in top), Vector()) / len(top)
    cap = bm.verts.new(centre + Vector((0.0, 0.0, -2.5)))
    colours.append(DARK)
    for s in range(segments):
        bm.faces.new((top[s], top[(s + 1) % segments], cap))
    bottom = verts[0]
    floor = bm.verts.new(sum((v.co for v in bottom), Vector()) / len(bottom))
    colours.append(DARK)
    for s in range(segments):
        bm.faces.new((bottom[(s + 1) % segments], bottom[s], floor))
    return finish("SM_Monolith_Bent", bm, colours, smooth=True)


bpy.ops.wm.read_factory_settings(use_empty=True)
for i in range(1, 7):
    slab_tower(i)
for i in range(1, 6):
    slab_fragment(i)
monolith()

for obj in bpy.context.scene.collection.objects:
    low = min(v.co.z for v in obj.data.vertices)
    high = max(v.co.z for v in obj.data.vertices)
    print("THALASSA: %s, %d vertices, %.1f m to %.1f m" % (obj.name, len(obj.data.vertices), low, high))

os.makedirs(os.path.dirname(out_path), exist_ok=True)
bpy.ops.export_scene.fbx(filepath=out_path, use_selection=False, object_types={"MESH"}, apply_unit_scale=True,
                         apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="OFF", colors_type="SRGB",
                         add_leaf_bones=False, bake_anim=False)
print("THALASSA: exported " + out_path)
