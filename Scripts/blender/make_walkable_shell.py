"""Builds Art/Ship/WalkableShell.blend from the grey boxes players walk on today, and exports its FBX.

Input is the JSON written by Scripts/unreal/export_greybox_boxes.py. Every walkable grey box becomes:
  - its own collision object, UBX_SM_WalkableShell_000 and so on. These are what players walk on.
  - part of SM_WalkableShell, one reference mesh of all of them, so the file shows something solid.

Never overwrites an existing .blend unless given --force: that file is the owner's to change.

Run with: blender --background --factory-startup --python make_walkable_shell.py -- <boxes.json> <Art/Ship folder> [--force]
"""
import json
import os
import sys

import bmesh
import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import walkable_shell_fbx  # noqa: E402

args = sys.argv[sys.argv.index("--") + 1:]
boxes_path, out_dir = args[0], args[1]
force = "--force" in args
blend_path = os.path.join(out_dir, "WalkableShell.blend")
if os.path.exists(blend_path) and not force:
    raise SystemExit("THALASSA: " + blend_path + " already exists; left as it is")

boxes = json.load(open(boxes_path, encoding="utf-8"))

# Unreal is left-handed, in centimetres: +X forward, +Y starboard, +Z up. Blender is right-handed, in metres.
# Mirroring Y converts one to the other, and the FBX exporter and importer undo it the same way.
def to_blender(p):
    return (p[0] / 100.0, -p[1] / 100.0, p[2] / 100.0)

# A cube's corners and faces, as signs along the box's own axes.
CORNERS = [(sx, sy, sz) for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]
FACES = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]

def box_mesh(name, box_list):
    bm = bmesh.new()
    for box in box_list:
        verts = [bm.verts.new(to_blender(c)) for c in box["corners"]]
        for face in FACES:
            bm.faces.new([verts[i] for i in face])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    return mesh

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = "METRIC"
scene.unit_settings.scale_length = 1.0

shell_collection = bpy.data.collections.new("Shell (reference, not collision)")
collision_collection = bpy.data.collections.new("Collision (UBX boxes: what players walk on)")
scene.collection.children.link(shell_collection)
scene.collection.children.link(collision_collection)

shell = bpy.data.objects.new("SM_WalkableShell", box_mesh("SM_WalkableShell", boxes))
shell_collection.objects.link(shell)

for index, box in enumerate(boxes):
    name = "UBX_SM_WalkableShell_%03d" % index
    obj = bpy.data.objects.new(name, box_mesh(name, [box]))
    obj.display_type = "WIRE"
    collision_collection.objects.link(obj)

os.makedirs(out_dir, exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=blend_path)
print("THALASSA: saved " + blend_path + " with %d collision boxes" % len(boxes))
walkable_shell_fbx.export(os.path.join(out_dir, "WalkableShell.fbx"))
