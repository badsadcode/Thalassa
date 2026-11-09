"""Slims a Meshy AI model for the game: at most 30,000 triangles, textures at most 2048 pixels.

Meshy's models can be a million triangles with 4K textures. In the murk, at the distances formations are seen,
that adds nothing but size. This reads one Meshy folder (the FBX and its PNGs), reduces the mesh with Blender's
collapse decimation (keeping its texture layout, so the textures still fit), sets its origin to the middle of its
base, and writes the FBX and resized textures (colour, normal, roughness; metallic is dropped) to the output folder.

Run with: python Scripts/build/blender.py Scripts/blender/slim_meshy.py -- <Meshy folder> <output folder> [max triangles]
"""
import glob
import os
import sys

import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index("--") + 1:]
source, target = args[0], args[1]
max_triangles = int(args[2]) if len(args) > 2 else 30000
MAX_TEXTURE = 2048

fbx = glob.glob(os.path.join(source, "*.fbx"))[0]
os.makedirs(target, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
for obj in meshes:
    bpy.context.view_layer.objects.active = obj
    triangles = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    if triangles > max_triangles:
        decimate = obj.modifiers.new("Slim", "DECIMATE")
        decimate.decimate_type = "COLLAPSE"
        decimate.ratio = max_triangles / triangles
        decimate.use_collapse_triangulate = True
        bpy.ops.object.modifier_apply(modifier=decimate.name)
    after = sum(len(p.vertices) - 2 for p in obj.data.polygons)
    # The origin at the middle of the base, so the game can set it on the ground.
    corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    base = Vector((sum(c.x for c in corners) / 8, sum(c.y for c in corners) / 8, min(c.z for c in corners)))
    bpy.context.scene.cursor.location = base
    obj.select_set(True)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    obj.location -= base
    print("SLIM: %s %d -> %d triangles" % (obj.name, triangles, after))

name = os.path.splitext(os.path.basename(fbx))[0]
bpy.ops.export_scene.fbx(filepath=os.path.join(target, name + ".fbx"), use_selection=False, object_types={"MESH"},
                         apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", mesh_smooth_type="FACE",
                         add_leaf_bones=False, bake_anim=False, path_mode="STRIP")

# The textures, resized with Blender's own image scaling (no extra Python packages needed).
for png in glob.glob(os.path.join(source, "*.png")):
    if png.lower().endswith("_metallic.png"):
        continue
    image = bpy.data.images.load(png)
    w, h = image.size
    if max(w, h) > MAX_TEXTURE:
        image.scale(MAX_TEXTURE, MAX_TEXTURE * h // w)
    image.filepath_raw = os.path.join(target, os.path.basename(png))
    image.file_format = "PNG"
    image.save()
    print("SLIM: %s %dx%d -> %dx%d" % (os.path.basename(png), w, h, image.size[0], image.size[1]))
