"""Generates the ocean's creatures as placeholder meshes, and exports them as FBX.

  SM_Glider   A big, peaceful gliding animal, about 5 m across the wings: a broad soft body, wings with a
              scalloped trailing edge, a long whip of a tail, and a pair of faint glowing spots near the wingtips.

Placeholder art, like the grey deck: shapes that read in the murk, made by rules, until the creatures are designed.

Conventions every creature mesh follows, for the creature material (create_creature_materials.py):
  - metres, +X forward, +Z up, origin at the middle of the body;
  - vertex colour RGB is the skin, exported linear (Unreal reads FBX colours as they are);
  - UV channel 1, U: where the vertex is on the moving part, 0 to 1 (out along a wing, or back along a body);
  - UV channel 2, U: the creature's size in metres, so motion scales with it;
  - UV channel 3, U: how it moves: 0 a body that sways from side to side, 1 wings that flap;
  - UV channel 4, U: how much it glows, 0 to 1 (vertex alpha doesn't survive the FBX export).
  Only U is used: the FBX importer flips V.

Deterministic: the same script always makes the same meshes. See docs/CREATURES.md.

Run with: python Scripts/build/blender.py Scripts/blender/make_creatures.py -- <output.fbx>
"""
import math
import os
import sys

import bpy
import bmesh
from mathutils import noise, Vector

out_path = sys.argv[sys.argv.index("--") + 1]


def mix(a, b, t):
    return tuple(a[i] * (1.0 - t) + b[i] * t for i in range(len(a)))


def smooth(a, b, x):
    t = max(0.0, min(1.0, (x - a) / (b - a)))
    return t * t * (3.0 - 2.0 * t)


class Builder:
    """A bmesh plus what each vertex carries: colour with glow, where it is on the moving part."""

    def __init__(self, size_m, mode):
        self.bm = bmesh.new()
        self.data = {}
        self.size_m = size_m
        self.mode = mode

    def vert(self, co, colour, glow, along):
        v = self.bm.verts.new(co)
        self.data[v] = ((*colour, glow), along)
        return v

    def face(self, *verts):
        return self.bm.faces.new(verts)

    def finish(self, name):
        bm = self.bm
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        uv0 = bm.loops.layers.uv.new("UVMap")
        uv1 = bm.loops.layers.uv.new("Along")
        uv2 = bm.loops.layers.uv.new("Size")
        uv3 = bm.loops.layers.uv.new("Mode")
        uv4 = bm.loops.layers.uv.new("Glow")
        for f in bm.faces:
            for loop in f.loops:
                colour, along = self.data[loop.vert]
                loop[uv0].uv = (loop.vert.co.x / self.size_m, loop.vert.co.y / self.size_m)
                loop[uv1].uv = (along, 0.0)
                loop[uv2].uv = (self.size_m, 0.0)
                loop[uv3].uv = (float(self.mode), 0.0)
                loop[uv4].uv = (colour[3], 0.0)
        bm.verts.index_update()
        colours = [self.data[v][0] for v in bm.verts]
        mesh = bpy.data.meshes.new(name)
        bm.to_mesh(mesh)
        bm.free()
        mesh.polygons.foreach_set("use_smooth", [True] * len(mesh.polygons))
        attribute = mesh.color_attributes.new("Col", "BYTE_COLOR", "POINT")
        for index, colour in enumerate(colours):
            attribute.data[index].color = (*colour[:3], 1.0)
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.scene.collection.objects.link(obj)
        return obj


def catmull(points, steps):
    """A closed Catmull-Rom curve through the points, steps per span."""
    out = []
    n = len(points)
    for i in range(n):
        p0, p1, p2, p3 = points[(i - 1) % n], points[i], points[(i + 1) % n], points[(i + 2) % n]
        for s in range(steps):
            t = s / steps
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * ((2 * p1[k]) + (-p0[k] + p2[k]) * t + (2 * p0[k] - 5 * p1[k] + 4 * p2[k] - p3[k]) * t2
                                    + (-p0[k] + 3 * p1[k] - 3 * p2[k] + p3[k]) * t3) for k in range(2)))
    return out


def glider():
    """Broad wings around a soft body, flat below, domed above, and a long thin tail."""
    span = 2.5      # Half the wingspan.
    body = 1.6      # Half the body's length, nose to the tail's root.
    b = Builder(size_m=span * 2.0, mode=1)

    # The outline, from the nose round the port wing to the tail's root, then mirrored. The trailing edge
    # is scalloped, like a soft fin, not a cut-out kite.
    port = [(body, 0.0), (body * 0.82, 0.55), (body * 0.35, 1.45), (-0.05, span), (-0.42, span * 0.82),
            (-0.55, span * 0.62), (-0.82, span * 0.52), (-0.95, span * 0.32), (-body * 0.82, 0.30)]
    starboard = [(x, -y) for (x, y) in reversed(port[1:])]
    outline = catmull(port + [(-body, 0.0)] + starboard, 5)

    back = (0.045, 0.06, 0.07)
    lines = (0.20, 0.27, 0.30)
    belly = (0.62, 0.66, 0.66)

    def skin(x, y, top):
        if not top:
            return belly, 0.0
        # Pale lines curving out along the wings, and mottling.
        swirl = math.sin((abs(y) * 2.6 - x * 1.4) * 2.2)
        c = mix(back, lines, smooth(0.86, 0.98, swirl) * 0.8)
        c = mix(c, back, 0.5 + 0.5 * noise.noise(Vector((x * 1.3, y * 1.3, 4.0))))
        # A glowing spot near each wingtip.
        tip = Vector((-0.12, math.copysign(span * 0.8, y)))
        glow = 1.0 - smooth(0.10, 0.20, (Vector((x, y)) - tip).length)
        return c, glow

    # Inside the edge the outline smooths out, so the scallops don't run as creases to the middle.
    soft = list(outline)
    for _ in range(12):
        soft = [tuple((soft[i - 1][k] + soft[i][k] * 2 + soft[(i + 1) % len(soft)][k]) / 4 for k in range(2)) for i in range(len(soft))]

    rings = 10
    top_rings, bottom_rings = [], []
    for k in range(rings):
        scale = 1.0 - k / rings
        shape = [mix(outline[i], soft[i], min(1.0, k / 2.0)) for i in range(len(outline))]
        for top in (True, False):
            ring = []
            for (x, y) in shape:
                px, py = x * scale, y * scale
                # Thick in the middle, thinning to a soft edge; the back domed, the belly nearly flat.
                inner = 1.0 - scale
                thick = (0.42 if top else -0.14) * (1.0 - (1.0 - inner) ** 2) * (1.0 - 0.55 * min(1.0, abs(py) / span))
                c, glow = skin(px, py, top)
                ring.append(b.vert((px, py, thick), c, glow, min(1.0, abs(py) / span)))
            (top_rings if top else bottom_rings).append(ring)
    centre_top = b.vert((0.0, 0.0, 0.46), back, 0.0, 0.0)
    centre_bottom = b.vert((0.0, 0.0, -0.15), belly, 0.0, 0.0)

    n = len(outline)
    for rings_of, centre, top in ((top_rings, centre_top, True), (bottom_rings, centre_bottom, False)):
        for k in range(rings - 1):
            for i in range(n):
                j = (i + 1) % n
                quad = (rings_of[k][i], rings_of[k][j], rings_of[k + 1][j], rings_of[k + 1][i])
                b.face(*(quad if top else reversed(quad)))
        last = rings_of[-1]
        for i in range(n):
            j = (i + 1) % n
            b.face(*((last[i], last[j], centre) if top else (centre, last[j], last[i])))
    # The edge: the outer top and bottom rings share their outline, so close it with a thin band.
    for i in range(n):
        j = (i + 1) % n
        b.face(top_rings[0][j], top_rings[0][i], bottom_rings[0][i], bottom_rings[0][j])

    # Two small eyes, set into the dome at the front.
    for side in (1.0, -1.0):
        eye = b.vert((body * 0.62, side * 0.38, 0.30), (0.01, 0.01, 0.01), 0.15, 0.1)
        near = [v for v in top_rings[5] if (v.co - eye.co).length < 0.6]
        near.sort(key=lambda v: math.atan2(v.co.y - eye.co.y, v.co.x - eye.co.x))
        # Eyes are simply dark: the nearest vertices of the dome take the colour.
        b.bm.verts.remove(eye)
        del b.data[eye]
        for v in near[:3]:
            colour, along = b.data[v]
            b.data[v] = ((0.01, 0.01, 0.01, colour[3]), along)

    # The tail: a thin tapering whip, three times the body's length, square in section.
    root = Vector((-body * 0.95, 0.0, 0.05))
    length = body * 3.2
    segments = 10
    prev = None
    for s in range(segments + 1):
        t = s / segments
        r = 0.09 * (1.0 - t) + 0.01
        x = root.x - length * t
        ring = [b.vert((x, r * dy, root.z + r * dz), back, 0.0, 0.0) for (dy, dz) in ((1, 0), (0, 1), (-1, 0), (0, -1))]
        if prev:
            for i in range(4):
                j = (i + 1) % 4
                b.face(prev[i], prev[j], ring[j], ring[i])
        prev = ring
    tip = b.vert((root.x - length - 0.05, 0.0, root.z), back, 0.0, 0.0)
    for i in range(4):
        b.face(prev[i], prev[(i + 1) % 4], tip)

    return b.finish("SM_Glider")


bpy.ops.wm.read_factory_settings(use_empty=True)
glider()

for obj in bpy.context.scene.collection.objects:
    xs = [v.co.x for v in obj.data.vertices]
    ys = [v.co.y for v in obj.data.vertices]
    print("THALASSA: %s, %d vertices, %d faces, %.1f by %.1f m" % (obj.name, len(obj.data.vertices), len(obj.data.polygons),
                                                                  max(xs) - min(xs), max(ys) - min(ys)))

os.makedirs(os.path.dirname(out_path), exist_ok=True)
bpy.ops.export_scene.fbx(filepath=out_path, use_selection=False, object_types={"MESH"}, apply_unit_scale=True,
                         apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="FACE", colors_type="LINEAR",
                         add_leaf_bones=False, bake_anim=False)
print("THALASSA: exported " + out_path)
