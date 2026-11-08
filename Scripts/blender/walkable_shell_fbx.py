"""Exports the open .blend as the walkable shell's FBX, with the settings Unreal expects.

Inside Blender (Scripting tab, open this file, Run Script) it writes WalkableShell.fbx next to the .blend.
From the command line: blender WalkableShell.blend --background --python walkable_shell_fbx.py
"""
import os

import bpy


def export(fbx_path):
    bpy.ops.export_scene.fbx(
        filepath=fbx_path,
        use_selection=False,
        object_types={"MESH"},
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_NONE",
        use_mesh_modifiers=True,
        mesh_smooth_type="FACE",
        add_leaf_bones=False,
        bake_anim=False,
    )
    print("THALASSA: exported " + fbx_path)


if __name__ == "__main__":
    export(os.path.join(os.path.dirname(bpy.data.filepath), "WalkableShell.fbx"))
