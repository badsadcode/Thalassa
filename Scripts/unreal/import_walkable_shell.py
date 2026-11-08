"""Imports Art/Ship/WalkableShell.fbx as /Game/Ship/Shell/SM_WalkableShell, replacing the asset if it exists.

Objects named UBX_SM_WalkableShell_... become its box collision. That collision is what players walk on.
Afterwards it reports how many collision boxes the mesh has, and with --check compares them, one by one,
with Saved/WalkableShell/greybox_boxes.json.

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript="<this file> [--check]" -unattended -nullrhi
"""
import json
import os
import sys
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
FBX = os.path.join(PROJECT, "Art", "Ship", "WalkableShell.fbx")
BOXES = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WalkableShell", "greybox_boxes.json")
ASSET = "/Game/Ship/Shell/SM_WalkableShell"

options = unreal.FbxImportUI()
options.import_mesh = True
options.import_materials = False
options.import_textures = False
options.import_animations = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
data = options.static_mesh_import_data
data.combine_meshes = True
data.auto_generate_collision = True  # Interchange reads this as "import collision"; UBX_ objects are used when present.
data.one_convex_hull_per_ucx = True
data.generate_lightmap_u_vs = False

task = unreal.AssetImportTask()
task.filename = FBX
task.destination_path = "/Game/Ship/Shell"
task.destination_name = "SM_WalkableShell"
task.replace_existing = True
task.automated = True
task.save = True
task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

mesh = unreal.load_asset(ASSET)
if not mesh:
    raise RuntimeError("THALASSA: import failed: " + FBX)
geom = mesh.get_editor_property("body_setup").get_editor_property("agg_geom")
found = list(geom.get_editor_property("box_elems"))
convex = len(geom.get_editor_property("convex_elems"))
unreal.log("THALASSA: imported %s with %d collision boxes and %d convex hulls" % (ASSET, len(found), convex))

if "--check" in sys.argv[-1] or "--check" in sys.argv:
    expected = json.load(open(BOXES, encoding="utf-8"))
    worst = 0.0
    unmatched = 0
    for box in expected:
        best = None
        for elem in found:
            c = elem.get_editor_property("center")
            d = ((c.x - box["center"][0]) ** 2 + (c.y - box["center"][1]) ** 2 + (c.z - box["center"][2]) ** 2) ** 0.5
            if best is None or d < best[0]:
                best = (d, elem)
        if best is None or best[0] > 1.0:
            unmatched += 1
            unreal.log("THALASSA: check: no collision box for the grey box at %s, size %s" % (box["center"], box["size"]))
            continue
        # Compare the corners, so a ramp that lost its slope or a box turned the wrong way shows up.
        e = best[1]
        half = unreal.Vector(e.get_editor_property("x") / 2, e.get_editor_property("y") / 2, e.get_editor_property("z") / 2)
        t = unreal.Transform(e.get_editor_property("center"), e.get_editor_property("rotation"), unreal.Vector(1, 1, 1))
        corners = [t.transform_location(unreal.Vector(sx * half.x, sy * half.y, sz * half.z)) for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]
        for want in box["corners"]:
            worst = max(worst, min(((p.x - want[0]) ** 2 + (p.y - want[1]) ** 2 + (p.z - want[2]) ** 2) ** 0.5 for p in corners))
    unreal.log("THALASSA: check: %d of %d grey boxes have no matching collision box; worst difference %.3f cm"
               % (unmatched, len(expected), worst))
