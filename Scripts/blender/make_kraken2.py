"""Turns the owner's kraken model (Art/Creatures/Kraken_Test.blend) into SM_KrakenElder for the game.

The source has a body (Mesh_0), eight limbs (geometry_0 to geometry_0.007) and two eye spheres (Sphere, Sphere.001),
about 2 m long, head towards -Y, the limbs trailing towards +Y, each about 30,000 triangles, with textures packed
in the file. This:
  - reduces the limbs to about 6,000 triangles each and the body to about 18,000;
  - writes what the creature materials read (see make_creatures.py): UV channel 1 where a vertex is along the body
    (0 at the snout, 0.38 at the back of the body) or along a limb (0.4 at its root to 1 at its tip), channel 2 the
    size (100 m), channel 3 the motion (4: kraken), channel 4 the glow (none: it doesn't glow, except its eyes);
  - maps the eye texture across the front of each eye sphere (UV channel 0), as an iris, horizontal slit forward;
  - joins it into one mesh with three material slots, Body, Limb and Eye; turns it to face +X; centres it on the
    body's axis; scales it to 100 m long;
  - saves the packed textures as PNGs beside the FBX.

Run with: python Scripts/build/blender.py Scripts/blender/make_kraken2.py -- <source .blend> <output folder>
"""
import math
import os
import sys

import bpy
import bmesh
from mathutils import Matrix, Vector

args = sys.argv[sys.argv.index("--") + 1:]
source, target = args[0], args[1]
LENGTH_M = 100.0
os.makedirs(target, exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=source)

objects = {o.name: o for o in bpy.data.objects if o.type == "MESH"}
body = objects["Mesh_0"]
limbs = [o for name, o in objects.items() if name.startswith("geometry_0")]
eyes = [o for name, o in objects.items() if name.startswith("Sphere")]

# Save the packed textures, under names that say what they are.
names = {"Image_0": "T_KrakenElder_BodyColour", "Image_1": "T_KrakenElder_BodyORM", "Image_2": "T_KrakenElder_BodyNormal",
         "Image_0.001": "T_KrakenElder_LimbColour"}
for image in bpy.data.images:
    if image.name in names:
        image.filepath_raw = os.path.join(target, names[image.name] + ".png")
        image.file_format = "PNG"
        image.save()
        print("KRAKEN2: saved %s (%dx%d)" % (names[image.name], image.size[0], image.size[1]))

for material, name in ((body.data.materials[0], "Body"), (limbs[0].data.materials[0], "Limb")):
    material.name = name
eye_material = bpy.data.materials.new("Eye")


def bake(o):
    """Applies the object's transform to its mesh."""
    o.data.transform(o.matrix_world)
    o.matrix_world = Matrix.Identity(4)


def decimate(o, ratio):
    bpy.context.view_layer.objects.active = o
    m = o.modifiers.new("Slim", "DECIMATE")
    m.decimate_type = "COLLAPSE"
    m.ratio = ratio
    m.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier=m.name)


for o in [body] + limbs + eyes:
    bake(o)
decimate(body, 0.6)
for limb in limbs:
    decimate(limb, 0.2)

# The body's extent along Y, and its axis, from its own vertices.
ys = [v.co.y for v in body.data.vertices]
head_y, tail_y = min(ys), max(ys)
axis = Vector((sum(v.co.x for v in body.data.vertices) / len(ys), 0.0, sum(v.co.z for v in body.data.vertices) / len(ys)))


def write_layers(o, along_of, glow=0.0, eye_uv_of=None):
    """Adds the four data channels (and, for an eye, its iris mapping in channel 0) and white vertex colours."""
    me = o.data
    bm = bmesh.new()
    bm.from_mesh(me)
    layers = [bm.loops.layers.uv.get(name) or bm.loops.layers.uv.new(name) for name in ("Along", "Size", "Mode", "Glow")]
    uv0 = bm.loops.layers.uv.active or bm.loops.layers.uv.new("UVMap")
    for f in bm.faces:
        for loop in f.loops:
            co = loop.vert.co
            loop[layers[0]].uv = (along_of(co), 0.0)
            loop[layers[1]].uv = (LENGTH_M, 0.0)
            loop[layers[2]].uv = (4.0, 0.0)
            loop[layers[3]].uv = (glow, 0.0)
            if eye_uv_of:
                loop[uv0].uv = eye_uv_of(co)
    bm.to_mesh(me)
    bm.free()
    colour = me.color_attributes.get("Col") or me.color_attributes.new("Col", "BYTE_COLOR", "POINT")
    for item in colour.data:
        item.color = (1.0, 1.0, 1.0, 1.0)


def body_along(co):
    return 0.38 * (co.y - head_y) / (tail_y - head_y)


write_layers(body, body_along)
for limb in limbs:
    ly = [v.co.y for v in limb.data.vertices]
    root, tip = min(ly), max(ly)
    write_layers(limb, lambda co, root=root, tip=tip: 0.4 + 0.6 * max(0.0, co.y - root) / (tip - root))
for eye in eyes:
    centre = sum((v.co for v in eye.data.vertices), Vector()) / len(eye.data.vertices)
    radius = max((v.co - centre).length for v in eye.data.vertices)
    # The iris faces outward from the body's axis; the slit runs along the body (Y in the source, forward later).
    write_layers(eye, body_along, eye_uv_of=lambda co, c=centre, r=radius: (0.5 - 0.48 * (co.y - c.y) / r, 0.5 + 0.48 * (co.z - c.z) / r))
    eye.data.materials.clear()
    eye.data.materials.append(eye_material)

# One mesh, three slots: Body, Limb, Eye.
for o in bpy.context.scene.objects:
    o.select_set(False)
for o in [body] + limbs + eyes:
    o.select_set(True)
bpy.context.view_layer.objects.active = body
bpy.ops.object.join()
kraken = body
kraken.name = "SM_KrakenElder"
kraken.data.name = "SM_KrakenElder"

# Head forward (+X), centred on the body's axis, 100 m long.
extent = max(v.co.y for v in kraken.data.vertices) - head_y
scale = LENGTH_M / extent
centre = Vector((axis.x, (head_y + max(v.co.y for v in kraken.data.vertices)) * 0.5, axis.z))
transform = Matrix.Scale(scale, 4) @ Matrix.Rotation(math.radians(90.0), 4, "Z") @ Matrix.Translation(-centre)
kraken.data.transform(transform)
kraken.data.update()

for o in list(bpy.context.scene.objects):
    if o is not kraken:
        bpy.data.objects.remove(o)
triangles = sum(len(p.vertices) - 2 for p in kraken.data.polygons)
xs = [v.co.x for v in kraken.data.vertices]
print("KRAKEN2: %d triangles, %.1f m long, slots %s" % (triangles, max(xs) - min(xs), [m.name for m in kraken.data.materials]))
bpy.ops.export_scene.fbx(filepath=os.path.join(target, "SM_KrakenElder.fbx"), use_selection=False, object_types={"MESH"},
                         apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="FACE",
                         colors_type="LINEAR", add_leaf_bones=False, bake_anim=False, path_mode="STRIP")
print("KRAKEN2: exported")
