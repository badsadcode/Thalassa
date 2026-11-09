"""Generates the ocean's creatures as placeholder meshes, and exports them as FBX.

  SM_Glider          A big, peaceful gliding animal, about 5 m across the wings: a broad soft body, wings with a
                     scalloped trailing edge, a long whip of a tail, and a faint glowing spot near each wingtip.
  SM_Fish_Dart       Small, slender, silver schooling fish, 0.3 m.
  SM_Fish_Disc       Deep-bodied, gold with dark bands, 0.4 m.
  SM_Fish_Lantern    Small, dark, big-headed, with glowing spots along the belly, 0.2 m.
  SM_Fish_Grouper    Heavy and mottled, near the ground, 1.4 m.
  SM_Fish_Eel        Long and thin, with ribbon fins, 2.2 m.
  SM_Fish_Cruiser    Big, slow, blunt-headed, long-finned, with a faint glowing line along each flank, 7 m.
  SM_Jelly_Giant     A pale jellyfish, a 6 m bell with tentacles to 14 m.
  SM_Jelly_Glow      A blue jellyfish, 2.2 m across, with a glowing rim and banded tentacles.
  SM_Jelly_Mote      A tiny glowing jellyfish, 0.3 m, for drifting swarms.
  SM_Leviathan_Head  The leviathan's head, 16 m long, with big glowing eyes.
  SM_Leviathan_Body  A body segment, placed 10 m apart (11.2 m long, so they overlap), scaled down towards the tail.
  SM_Leviathan_Tail  The tail, 22 m, ending in an upright fluke.

Placeholder art, like the grey deck: shapes that read in the murk, made by rules, until the creatures are designed.

Conventions every creature mesh follows, for the creature material (import_creatures.py):
  - metres, +X forward, +Z up, origin at the middle of the body;
  - vertex colour RGB is the skin, exported linear (Unreal reads FBX colours as they are);
  - UV channel 1, U: where the vertex is on the moving part, 0 to 1 (out along a wing, or back along a body);
  - UV channel 2, U: the creature's size in metres, so motion scales with it;
  - UV channel 3, U: how it moves: 0 a body that sways from side to side, 1 wings that flap, 2 a jellyfish's
    bell pulsing and tentacles trailing, 3 not at all (the leviathan's pieces move by where they are put);
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
    """A bmesh plus what each vertex carries: colour with glow, where it is on the moving part. Faces can use the
    second material slot (material=1: the eyes, drawn with an eye texture through UV channel 0)."""

    def __init__(self, size_m, mode):
        self.bm = bmesh.new()
        self.data = {}
        self.uv = {}
        self.size_m = size_m
        self.mode = mode
        self.has_eyes = False

    def vert(self, co, colour, glow, along, uv=None):
        v = self.bm.verts.new(co)
        self.data[v] = ((*colour, glow), along)
        if uv is not None:
            self.uv[v] = uv
        return v

    def face(self, *verts, material=0):
        f = self.bm.faces.new(verts)
        f.material_index = material
        self.has_eyes |= material == 1
        return f

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
                loop[uv0].uv = self.uv.get(loop.vert, (loop.vert.co.x / self.size_m, loop.vert.co.y / self.size_m))
                loop[uv1].uv = (along, 0.0)
                loop[uv2].uv = (self.size_m, 0.0)
                loop[uv3].uv = (float(self.mode), 0.0)
                loop[uv4].uv = (colour[3], 0.0)
        bm.verts.index_update()
        colours = [self.data[v][0] for v in bm.verts]
        mesh = bpy.data.meshes.new(name)
        bm.to_mesh(mesh)
        bm.free()
        mesh.materials.append(bpy.data.materials.get("Body") or bpy.data.materials.new("Body"))
        if self.has_eyes:
            mesh.materials.append(bpy.data.materials.get("Eye") or bpy.data.materials.new("Eye"))
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


def loft(b, stations, segments, colour_fn, glow_fn, along_fn, cap_front=True, cap_back=True):
    """A tube around the X axis through stations of (x, half width, half height, z offset), front to back.
    Returns the rings of vertices. colour_fn(t, a, x) -> rgb, glow_fn(t, a, x) -> 0..1, along_fn(t) -> 0..1."""
    rings = []
    n = len(stations)
    for k, (x, hw, hh, zo) in enumerate(stations):
        t = k / (n - 1)
        ring = []
        for s in range(segments):
            a = 2.0 * math.pi * s / segments
            co = (x, hw * math.cos(a), zo + hh * math.sin(a))
            ring.append(b.vert(co, colour_fn(t, a, x), glow_fn(t, a, x), along_fn(t)))
        rings.append(ring)
    for k in range(n - 1):
        for s in range(segments):
            j = (s + 1) % segments
            b.face(rings[k][s], rings[k + 1][s], rings[k + 1][j], rings[k][j])
    if cap_front:
        x, _, _, zo = stations[0]
        tip = b.vert((x + 0.001, 0.0, zo), colour_fn(0.0, 0.0, x), glow_fn(0.0, 0.0, x), along_fn(0.0))
        for s in range(segments):
            b.face(rings[0][(s + 1) % segments], rings[0][s], tip)
    if cap_back:
        x, _, _, zo = stations[-1]
        tip = b.vert((x - 0.001, 0.0, zo), colour_fn(1.0, 0.0, x), glow_fn(1.0, 0.0, x), along_fn(1.0))
        for s in range(segments):
            b.face(rings[-1][s], rings[-1][(s + 1) % segments], tip)
    return rings


def fin(b, points, colour, along, glow=0.0):
    """A flat fin through three or four points (drawn from both sides by the material)."""
    verts = [b.vert(p, colour, glow, along[i]) for i, p in enumerate(points)]
    b.face(*verts)
    return verts


def fish(name, length, height, width, back, belly, *, rings=16, segments=12, head=0.6, pattern=None, glow=None,
         tail=0.5, dorsal=(0.3, 0.6, 0.35), anal=(0.62, 0.78, 0.2), pectoral=0.18, fin_colour=None):
    """A fish: a lofted body from nose to the base of the tail, a forked tail fin, a dorsal and an anal fin, and
    pectoral fins. Sizes in metres. head: how far forward the body is deepest (smaller: a blunter, bigger head)."""
    b = Builder(size_m=length, mode=0)
    body = 0.8                          # The body is the front 80%; the tail fin the rest.
    nose = length * 0.5
    fin_colour = fin_colour or mix(back, (0.0, 0.0, 0.0), 0.3)

    def profile(t):
        return math.sin(math.pi * min(t ** head, 0.93)) ** 0.8

    def colour(t, a, x):
        up = math.sin(a)
        c = mix(belly, back, smooth(-0.35, 0.45, up))
        if pattern:
            c = pattern(t, a, x, c)
        # Eyes: dark, on either side of the head.
        if 0.06 < t < 0.16 and abs(math.cos(a)) > 0.8 and 0.0 < up < 0.55:
            c = (0.01, 0.01, 0.01)
        return c

    def glow_at(t, a, x):
        return glow(t, a, x) if glow else 0.0

    stations = []
    for k in range(rings):
        t = k / (rings - 1)
        f = profile(t)
        stations.append((nose - t * length * body, width * 0.5 * f, height * 0.5 * f, 0.0))
    loft(b, stations, segments, colour, glow_at, lambda t: t * body)

    def top(t):
        return height * 0.5 * profile(t)

    def x_at(t):
        return nose - t * length * body

    # The tail fin: forked, from the narrow base of the tail to two tips, with a notch between them.
    root = height * 0.5 * profile(1.0)
    end = -length * 0.5
    th = height * tail
    notch = (end + length * 0.07, 0.0, 0.0)
    fin(b, [(x_at(1.0), 0.0, root), (end, 0.0, th), notch], fin_colour, [body, 1.0, 0.95])
    fin(b, [(x_at(1.0), 0.0, -root), (x_at(1.0), 0.0, root), notch], fin_colour, [body, body, 0.95])
    fin(b, [(x_at(1.0), 0.0, -root), notch, (end, 0.0, -th)], fin_colour, [body, 0.95, 1.0])
    # Dorsal and anal fins: from where they start along the body to where they end, peaking behind the start.
    if dorsal:
        t0, t1, h = dorsal
        apex_t = t0 + (t1 - t0) * 0.35
        fin(b, [(x_at(t0), 0.0, top(t0) * 0.95), (x_at(apex_t) - length * 0.05, 0.0, top(apex_t) + height * h),
                (x_at(t1), 0.0, top(t1) * 0.95)], fin_colour, [t0 * body, apex_t * body, t1 * body])
    if anal:
        t0, t1, h = anal
        apex_t = t0 + (t1 - t0) * 0.4
        fin(b, [(x_at(t0), 0.0, -top(t0) * 0.95), (x_at(t1), 0.0, -top(t1) * 0.95),
                (x_at(apex_t) - length * 0.04, 0.0, -top(apex_t) - height * h)], fin_colour, [t0 * body, t1 * body, apex_t * body])
    # Pectoral fins, behind the head, swept back and a little down.
    if pectoral:
        t = 0.22
        side = width * 0.5 * profile(t)
        for s in (1.0, -1.0):
            fin(b, [(x_at(t), s * side * 0.9, -height * 0.08), (x_at(t + 0.08), s * side * 0.9, -height * 0.1),
                    (x_at(t + 0.1) - length * pectoral * 0.6, s * (side + length * pectoral), -height * 0.25)],
                fin_colour, [t * body, (t + 0.08) * body, (t + 0.1) * body])
    return b.finish(name)


def eel(name, length, thickness, back, belly):
    """A long eel-like fish: a body tapering to a point, one continuous fin along the back and one under the tail."""
    b = Builder(size_m=length, mode=0)
    rings = 40
    nose = length * 0.5

    def profile(t):
        return min(1.0, (t + 0.01) / 0.07) ** 0.5 * (1.0 - t ** 2.2) * 0.95 + 0.05

    def colour(t, a, x):
        c = mix(belly, back, smooth(-0.5, 0.3, math.sin(a)))
        c = mix(c, back, 0.4 + 0.4 * noise.noise(Vector((x * 3.0, a, 1.0))))
        if 0.02 < t < 0.05 and abs(math.cos(a)) > 0.75 and math.sin(a) > 0.0:
            c = (0.85, 0.8, 0.4)    # Pale eyes.
        return c

    stations = [(nose - t * length, thickness * 0.5 * profile(t), thickness * 0.55 * profile(t), 0.0)
                for t in (k / (rings - 1) for k in range(rings))]
    loft(b, stations, 8, colour, lambda t, a, x: 0.0, lambda t: t)
    fin_c = mix(back, (0.0, 0.0, 0.0), 0.2)
    # The ribbon fins: a strip along the top from a fifth of the way back to the tip, and one under the back half.
    for (t0, sign) in ((0.2, 1.0), (0.55, -1.0)):
        prev = None
        steps = 24
        for i in range(steps + 1):
            t = t0 + (1.0 - t0) * i / steps
            x = nose - t * length
            base = sign * thickness * 0.55 * profile(t) * 0.9
            h = sign * thickness * 0.45 * math.sin(math.pi * min(1.0, (t - t0) / (1.0 - t0) * 1.1))
            pair = (b.vert((x, 0.0, base), fin_c, 0.0, t), b.vert((x, 0.0, base + h), fin_c, 0.0, t))
            if prev:
                b.face(prev[0], pair[0], pair[1], prev[1])
            prev = pair
    return b.finish(name)


def jelly(name, radius, bell_height, tentacles, tentacle_length, bell_colour, tentacle_colour, *, glow_rim=0.3,
          glow_bands=0.0, arms=4, segments=28):
    """A jellyfish: a bell open below with a scalloped rim, tentacles hanging from the rim, and broad, ruffled arms
    from the middle. Origin at the bell's rim. UV 1 runs from the top of the bell (0) through the rim (0.15) to
    the tips of the tentacles (1)."""
    b = Builder(size_m=radius * 2.0, mode=2)
    lat = 9
    rings = []
    edge = math.cos(math.radians(100.0))
    for k in range(1, lat + 1):
        phi = math.radians(100.0) * k / lat
        r = radius * math.sin(phi) * (1.0 + 0.05 * k / lat)
        z = bell_height * (math.cos(phi) - edge)
        ring = []
        for s in range(segments):
            a = 2.0 * math.pi * s / segments
            lobe = 1.0 + (0.06 * math.cos(a * tentacles) if k == lat else 0.0)
            g = glow_rim * smooth(0.6, 1.0, k / lat)
            c = mix(bell_colour, tentacle_colour, k / lat * 0.5)
            ring.append(b.vert((r * lobe * math.cos(a), r * lobe * math.sin(a), z), c, g, 0.15 * k / lat))
        rings.append(ring)
    top = b.vert((0.0, 0.0, bell_height * (1.0 - edge)), bell_colour, 0.0, 0.0)
    for s in range(segments):
        b.face(rings[0][s], rings[0][(s + 1) % segments], top)
    for k in range(len(rings) - 1):
        for s in range(segments):
            j = (s + 1) % segments
            b.face(rings[k][s], rings[k + 1][s], rings[k + 1][j], rings[k][j])

    def strip(start, direction_out, length, width, colour, steps, ruffle=0.0, band=0.0):
        """A ribbon hanging down from a point, facing outward, tapering, curling a little as it goes."""
        prev = None
        side = Vector((-direction_out.y, direction_out.x, 0.0))
        for i in range(steps + 1):
            s = i / steps
            w = width * (1.0 - 0.8 * s) * (1.0 + ruffle * math.sin(s * 40.0))
            p = Vector(start) + Vector((0.0, 0.0, -length * s)) + direction_out * (length * 0.06 * math.sin(s * 3.0))
            g = band * (1.0 if (s * 6.0) % 1.0 < 0.3 else 0.15)
            u = 0.15 + 0.85 * s
            pair = (b.vert(p - side * w * 0.5, colour, g, u), b.vert(p + side * w * 0.5, colour, g, u))
            if prev:
                b.face(prev[0], pair[0], pair[1], prev[1])
            prev = pair

    rim_r = radius * math.sin(math.radians(100.0)) * 1.05
    for i in range(tentacles):
        a = 2.0 * math.pi * (i + 0.5) / tentacles
        out = Vector((math.cos(a), math.sin(a), 0.0))
        strip((rim_r * 0.97 * math.cos(a), rim_r * 0.97 * math.sin(a), 0.0), out,
              tentacle_length * (0.75 + 0.25 * ((i * 7) % 5) / 4.0), radius * 0.05, tentacle_colour, 14, band=glow_bands)
    for i in range(arms):
        a = 2.0 * math.pi * i / arms + 0.4
        out = Vector((math.cos(a), math.sin(a), 0.0))
        strip((radius * 0.12 * math.cos(a), radius * 0.12 * math.sin(a), bell_height * 0.25), out,
              tentacle_length * 0.55, radius * 0.35, mix(bell_colour, tentacle_colour, 0.3), 16, ruffle=0.35,
              band=glow_bands * 0.5)
    return b.finish(name)


# The leviathan: a head, a body segment repeated along its length (scaled smaller towards the tail), and a tail.
# Each piece is lofted round the X axis; the ends overlap their neighbours, so the joins don't show.
LEV_BACK = (0.022, 0.026, 0.03)
LEV_BELLY = (0.09, 0.095, 0.09)
LEV_R = 4.0     # Body radius, metres, at scale 1.


def lev_skin(t, a, x):
    up = math.sin(a)
    c = mix(LEV_BELLY, LEV_BACK, smooth(-0.5, 0.3, up))
    c = mix(c, LEV_BACK, 0.35 + 0.35 * noise.noise(Vector((x * 0.25, a * 1.5, 7.0))))
    # Old pale scars.
    if noise.noise(Vector((x * 0.6, a * 3.0, 3.0))) > 0.55:
        c = mix(c, (0.3, 0.3, 0.28), 0.6)
    return c


def lev_lights(x, a):
    """Two rows of glowing spots along each flank, spaced along the body."""
    side = abs(math.cos(a))
    row = (side > 0.92 and math.sin(a) > -0.15) or (0.6 < side < 0.75 and math.sin(a) < 0.0)
    return 1.0 if row and (x * 0.8) % 1.0 < 0.3 else 0.0


def lev_head():
    b = Builder(size_m=16.0, mode=3)
    rings, segments = 18, 28
    front, back = 9.0, -7.5

    def r(t):
        return LEV_R * (0.18 + 0.82 * min(1.0, t / 0.6) ** 0.55) * 1.04

    def colour(t, a, x):
        c = lev_skin(t, a, x)
        if -0.38 < math.sin(a) < -0.25 and t < 0.6:
            c = (0.005, 0.005, 0.005)     # The mouth's line.
        return c

    def glow(t, a, x):
        g = lev_lights(x, a) * smooth(0.45, 0.7, t)
        if t < 0.55 and -0.5 < math.sin(a) < -0.4:
            g = 0.6 if (x * 1.5) % 1.0 < 0.4 else 0.0    # Small lights along the jaw.
        return g

    stations = [(front - t * (front - back), r(t), r(t) * 0.82, -0.3 * (1.0 - t)) for t in (k / (rings - 1) for k in range(rings))]
    loft(b, stations, segments, colour, glow, lambda t: 0.0, cap_back=False)
    # The eyes: big, pale, glowing, bulging from either side of the head. What you see through the window.
    for side in (1.0, -1.0):
        centre = Vector((3.2, side * r(0.36) * 0.93, 0.9))
        eye_r = 0.9
        lat, seg = 10, 24
        prev = None
        for k in range(lat + 1):
            phi = math.pi * 0.5 * k / lat
            ring = []
            for s in range(seg):
                a = 2.0 * math.pi * s / seg
                local = Vector((eye_r * math.sin(phi) * math.cos(a), side * eye_r * math.cos(phi), eye_r * math.sin(phi) * math.sin(a)))
                # The iris texture spread across the eye: its middle at the front of the bulge.
                uv = (0.5 + 0.48 * math.sin(phi) * math.cos(a), 0.5 + 0.48 * math.sin(phi) * math.sin(a))
                ring.append(b.vert(centre + local, (0.9, 0.75, 0.35), 1.0, 0.0, uv))
            if prev:
                for s in range(seg):
                    j = (s + 1) % seg
                    quad = (prev[s], ring[s], ring[j], prev[j])
                    b.face(*(quad if side > 0 else tuple(reversed(quad))), material=1)
            else:
                apex = b.vert(centre + Vector((0.0, side * eye_r * 1.02, 0.0)), (0.9, 0.75, 0.35), 1.0, 0.0, (0.5, 0.5))
                for s in range(seg):
                    tri = (ring[(s + 1) % seg], ring[s], apex)
                    b.face(*(tri if side > 0 else tuple(reversed(tri))), material=1)
            prev = ring
    return b.finish("SM_Leviathan_Head")


def lev_body():
    b = Builder(size_m=11.2, mode=3)
    rings, segments = 12, 28
    stations = [(5.6 - t * 11.2, LEV_R * (1.0 + 0.04 * math.sin(math.pi * t)), LEV_R * 0.82 * (1.0 + 0.04 * math.sin(math.pi * t)), 0.0)
                for t in (k / (rings - 1) for k in range(rings))]
    loft(b, stations, segments, lev_skin, lambda t, a, x: lev_lights(x, a), lambda t: 0.0, cap_front=False, cap_back=False)
    fin_c = mix(LEV_BACK, (0.0, 0.0, 0.0), 0.3)
    # A ridge of spines along the back, and a pair of trailing fins low on the flanks.
    fin(b, [(3.5, 0.0, LEV_R * 0.8), (-1.5, 0.0, LEV_R * 0.8 + 2.6), (-3.5, 0.0, LEV_R * 0.8)], fin_c, [0.0, 0.0, 0.0])
    for s in (1.0, -1.0):
        fin(b, [(2.0, s * LEV_R * 0.8, -LEV_R * 0.45), (-1.0, s * LEV_R * 0.85, -LEV_R * 0.5), (-4.5, s * (LEV_R + 3.2), -LEV_R * 1.1)],
            fin_c, [0.0, 0.0, 0.0])
    return b.finish("SM_Leviathan_Body")


def lev_tail():
    b = Builder(size_m=22.0, mode=3)
    rings, segments = 16, 24
    front, back = 6.0, -16.0
    stations = [(front - t * (front - back), LEV_R * (1.0 - 0.92 * t ** 1.3), LEV_R * 0.82 * (1.0 - 0.9 * t ** 1.3), 0.0)
                for t in (k / (rings - 1) for k in range(rings))]
    loft(b, stations, segments, lev_skin, lambda t, a, x: lev_lights(x, a) * (1.0 - t), lambda t: 0.0, cap_front=False)
    fin_c = mix(LEV_BACK, (0.0, 0.0, 0.0), 0.2)
    # A tall, thin fluke, upright, like a sea snake's.
    fin(b, [(back + 6.0, 0.0, 0.4), (back - 2.5, 0.0, 5.5), (back - 1.0, 0.0, 0.0)], fin_c, [0.0, 0.0, 0.0])
    fin(b, [(back + 6.0, 0.0, -0.4), (back - 1.0, 0.0, 0.0), (back - 2.5, 0.0, -5.0)], fin_c, [0.0, 0.0, 0.0])
    return b.finish("SM_Leviathan_Tail")


def all_fish():
    # Darts: small, slender, silver, in big schools.
    fish("SM_Fish_Dart", 0.3, 0.065, 0.04, (0.10, 0.16, 0.20), (0.70, 0.74, 0.76),
         pattern=lambda t, a, x, c: (0.04, 0.05, 0.06) if abs(math.sin(a)) < 0.12 and 0.2 < t < 0.9 else c,
         dorsal=(0.35, 0.55, 0.3), anal=(0.6, 0.75, 0.15))
    # Discs: deep-bodied, gold with dark bands, tall fins.
    fish("SM_Fish_Disc", 0.42, 0.34, 0.07, (0.42, 0.33, 0.14), (0.62, 0.55, 0.38), head=0.5,
         pattern=lambda t, a, x, c: (0.04, 0.04, 0.05) if math.sin(t * math.pi * 7.0) > 0.55 else c,
         dorsal=(0.2, 0.75, 0.35), anal=(0.45, 0.78, 0.3), tail=0.4)
    # Lanterns: small, dark, big-headed, with rows of glowing spots along the belly.
    fish("SM_Fish_Lantern", 0.2, 0.06, 0.045, (0.03, 0.035, 0.045), (0.08, 0.09, 0.1), head=0.35,
         glow=lambda t, a, x: 1.0 if math.sin(a) < -0.35 and 0.15 < t < 0.9 and (t * 12.0) % 1.0 < 0.4 else 0.0,
         dorsal=(0.45, 0.6, 0.25), anal=(0.6, 0.75, 0.2))
    # Groupers: heavy, mottled, lurking near the ground.
    fish("SM_Fish_Grouper", 1.4, 0.45, 0.38, (0.16, 0.14, 0.11), (0.42, 0.40, 0.34), head=0.5,
         pattern=lambda t, a, x, c: mix(c, (0.5, 0.48, 0.42), 0.6) if noise.noise(Vector((x * 9.0, a * 2.5, 2.0))) > 0.35 else c,
         dorsal=(0.25, 0.7, 0.2), anal=(0.6, 0.8, 0.15), tail=0.35, pectoral=0.12)
    # Cruisers: big, slow, blunt-headed, with long pectoral fins and a faint glowing line along each flank.
    fish("SM_Fish_Cruiser", 7.0, 1.5, 1.3, (0.07, 0.09, 0.10), (0.48, 0.50, 0.50), rings=20, segments=16, head=0.45,
         glow=lambda t, a, x: 0.45 if abs(math.sin(a)) < 0.1 and 0.2 < t < 0.95 else 0.0,
         dorsal=(0.3, 0.48, 0.35), anal=(0.7, 0.8, 0.12), tail=0.75, pectoral=0.25)
    eel("SM_Fish_Eel", 2.2, 0.13, (0.10, 0.11, 0.07), (0.30, 0.30, 0.20))


def comb(name, length, radius, rows=8):
    """A comb jelly: an egg-shaped body with no tentacles, and rows of glowing combs running from top to bottom.
    Upright, origin at its middle. All of it counts as 'bell' (UV 1 under 0.15), so it pulses gently."""
    b = Builder(size_m=length, mode=2)
    lat, segments = 14, rows * 4
    rings = []
    for k in range(1, lat):
        t = k / lat
        z = length * (0.5 - t)
        r = radius * math.sin(math.pi * t) ** 0.8
        ring = []
        for s in range(segments):
            a = 2.0 * math.pi * s / segments
            on_row = s % 4 == 0
            ring.append(b.vert((r * math.cos(a), r * math.sin(a), z), (0.85, 0.88, 0.9), 1.0 if on_row and 0.15 < t < 0.85 else 0.0, 0.14 * t))
        rings.append(ring)
    top = b.vert((0.0, 0.0, length * 0.5), (0.85, 0.88, 0.9), 0.0, 0.0)
    bottom = b.vert((0.0, 0.0, -length * 0.5), (0.85, 0.88, 0.9), 0.0, 0.14)
    for s in range(segments):
        j = (s + 1) % segments
        b.face(rings[0][s], rings[0][j], top)
        b.face(rings[-1][j], rings[-1][s], bottom)
    for k in range(len(rings) - 1):
        for s in range(segments):
            j = (s + 1) % segments
            b.face(rings[k][s], rings[k + 1][s], rings[k + 1][j], rings[k][j])
    return b.finish(name)


def all_jellies():
    # Jellyfish are pale in their meshes: each school is tinted, and glows or not, by the game (see ThalassaOceanFauna.cpp).
    pale, rim = (0.85, 0.82, 0.82), (0.95, 0.93, 0.92)
    jelly("SM_Jelly_Giant", 3.0, 2.2, 20, 14.0, pale, rim, glow_rim=0.5, glow_bands=0.3, segments=32)
    jelly("SM_Jelly_Glow", 1.1, 0.9, 12, 6.0, pale, rim, glow_rim=1.0, glow_bands=1.0)
    jelly("SM_Jelly_Mote", 0.15, 0.13, 6, 0.4, pale, rim, glow_rim=1.0, glow_bands=0.8, arms=0, segments=10)
    # A helmet jelly: a tall bell, short tentacles, a bright ring round the rim.
    jelly("SM_Jelly_Helmet", 0.75, 1.6, 16, 1.4, pale, rim, glow_rim=1.0, glow_bands=0.0, arms=0, segments=24)
    comb("SM_Jelly_Comb", 1.2, 0.45)


def kraken():
    """A giant squid-like animal, about 60 m from the tip of its mantle to the end of its two long tentacles. It swims
    mantle first (+X), arms trailing. UV 1 runs from the mantle's tip (0) through the collar (0.3) and head (0.38) to
    the tips of the arms (1). Dark red-brown, with pale spots that glow faintly, and big, faintly glowing eyes."""
    b = Builder(size_m=60.0, mode=4)
    back, belly, spot = (0.30, 0.06, 0.045), (0.52, 0.22, 0.16), (0.85, 0.72, 0.62)

    def skin(t, a, x):
        c = mix(belly, back, smooth(-0.6, 0.4, math.sin(a)))
        n = noise.noise(Vector((x * 0.35, a * 2.0, 5.0)))
        return spot if n > 0.42 else mix(c, back, 0.3 + 0.3 * n)

    def spots(t, a, x):
        return 0.7 if noise.noise(Vector((x * 0.35, a * 2.0, 5.0))) > 0.42 else 0.0

    # The mantle: pointed at the tip, widest towards the collar.
    rings, segments = 18, 20
    stations = []
    for k in range(rings):
        t = k / (rings - 1)
        r = 4.6 * (0.06 + 0.94 * t ** 0.55) * (1.0 - 0.12 * smooth(0.85, 1.0, t))
        stations.append((22.0 - 22.0 * t, r, r * 0.92, 0.0))
    loft(b, stations, segments, skin, spots, lambda t: 0.3 * t, cap_back=False)
    # Fins: two broad flaps near the tip.
    for s in (1.0, -1.0):
        fin(b, [(21.0, s * 0.6, 0.0), (12.5, s * 3.3, 0.0), (15.5, s * 9.0, 0.0)], back, [0.02, 0.1, 0.06])
    # The head, behind the collar, narrower, with the eyes.
    head = [(0.4, 4.0, 3.7, 0.0), (-1.5, 3.9, 3.6, 0.0), (-3.5, 3.6, 3.3, 0.0), (-5.5, 2.9, 2.7, 0.0), (-6.5, 2.4, 2.2, 0.0)]
    loft(b, head, segments, skin, lambda t, a, x: 0.0, lambda t: 0.3 + 0.08 * t, cap_front=False, cap_back=False)
    for side in (1.0, -1.0):
        centre = Vector((-2.6, side * 3.55, 0.6))
        eye_r = 1.7
        lat, seg = 10, 24
        prev = None
        for k in range(lat + 1):
            phi = math.pi * 0.5 * k / lat
            ring = []
            for s in range(seg):
                a = 2.0 * math.pi * s / seg
                local = Vector((eye_r * math.sin(phi) * math.cos(a), side * eye_r * math.cos(phi) * 0.6, eye_r * math.sin(phi) * math.sin(a)))
                uv = (0.5 + 0.48 * math.sin(phi) * math.cos(a), 0.5 + 0.48 * math.sin(phi) * math.sin(a))
                ring.append(b.vert(centre + local, (0.95, 0.85, 0.45), 0.5, 0.34, uv))
            if prev:
                for s in range(seg):
                    j = (s + 1) % seg
                    quad = (prev[s], ring[s], ring[j], prev[j])
                    b.face(*(quad if side > 0 else tuple(reversed(quad))), material=1)
            else:
                apex = b.vert(centre + Vector((0.0, side * eye_r * 0.62, 0.0)), (0.95, 0.85, 0.45), 0.5, 0.34, (0.5, 0.5))
                for s in range(seg):
                    tri = (ring[(s + 1) % seg], ring[s], apex)
                    b.face(*(tri if side > 0 else tuple(reversed(tri))), material=1)
            prev = ring

    def limb(angle, length, base_r, tip_r, club=False):
        """An arm: a tapering tube from the head trailing back, spreading a little, curling at the tip."""
        out = Vector((0.0, math.cos(angle), math.sin(angle)))
        side = Vector((0.0, -math.sin(angle), math.cos(angle)))
        steps, sides = 20, 6
        rings_ = []
        for i in range(steps + 1):
            s = i / steps
            centre = Vector((-6.0 - length * s, 0.0, 0.0)) + out * (2.0 + length * 0.12 * math.sin(s * math.pi * 0.8)) \
                + side * (length * 0.05 * math.sin(s * 4.0 + angle))
            r = base_r * (1.0 - s) + tip_r * s
            if club and s > 0.8:
                r *= 1.0 + 1.2 * math.sin(math.pi * (s - 0.8) / 0.2)
            ring = []
            for j in range(sides):
                a = 2.0 * math.pi * j / sides
                normal = out * math.cos(a) + side * math.sin(a)
                c = mix(belly, back, 0.5 + 0.5 * math.cos(a))
                if math.cos(a) < -0.5:
                    c = mix(c, spot, 0.5)   # Paler undersides: the suckers' rows.
                ring.append(b.vert(centre + normal * r, c, 0.0, 0.4 + 0.6 * s))
            rings_.append(ring)
        for i in range(steps):
            for j in range(sides):
                k = (j + 1) % sides
                b.face(rings_[i][j], rings_[i + 1][j], rings_[i + 1][k], rings_[i][k])
        tip = b.vert(rings_[-1][0].co * 0.0 + sum((v.co for v in rings_[-1]), Vector()) / sides + Vector((-0.3, 0.0, 0.0)), back, 0.0, 1.0)
        for j in range(sides):
            b.face(rings_[-1][j], rings_[-1][(j + 1) % sides], tip)

    for i in range(8):
        limb(2.0 * math.pi * (i + 0.5) / 8.0, 30.0 + 3.0 * ((i * 5) % 3), 1.0, 0.08)
    for angle in (-math.pi * 0.5 + 0.35, -math.pi * 0.5 - 0.35):
        limb(angle, 52.0, 0.5, 0.12, club=True)
    return b.finish("SM_Kraken")


bpy.ops.wm.read_factory_settings(use_empty=True)
glider()
all_fish()
all_jellies()
lev_head()
lev_body()
lev_tail()
kraken()

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
