"""Writes the grey boxes players walk on to Saved/WalkableShell/greybox_boxes.json, for Scripts/blender/make_walkable_shell.py.

Each box is its eight corners in ship coordinates (centimetres, +X forward, +Y starboard, +Z up), and its
centre and size along its own axes, so the import can be checked box by box.

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript=<this file> -unattended -nullrhi
"""
import json
import os
import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WalkableShell", "greybox_boxes.json")

boxes = []
for t in unreal.ThalassaVessel.get_greybox_collision_transforms():
    corners = []
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (-1, 1):
                p = t.transform_location(unreal.Vector(sx * 50.0, sy * 50.0, sz * 50.0))
                corners.append([p.x, p.y, p.z])
    s = t.scale3d
    if min(s.x, s.y, s.z) * 100.0 < 0.1:
        continue  # A zero-width sliver where a doorway meets the end of a wall: no volume, so nothing to walk on.
    boxes.append({"corners": corners, "center": [t.translation.x, t.translation.y, t.translation.z],
                  "size": [s.x * 100.0, s.y * 100.0, s.z * 100.0]})

os.makedirs(os.path.dirname(OUT), exist_ok=True)
json.dump(boxes, open(OUT, "w", encoding="utf-8"), indent=1)
unreal.log("THALASSA: wrote %d boxes to %s" % (len(boxes), OUT))
