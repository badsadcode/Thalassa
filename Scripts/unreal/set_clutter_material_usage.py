"""Marks every material the ocean's rocks use as usable on instanced meshes and with Nanite.

The ocean draws its rocks as instances (UThalassaOceanClutter), many of them Nanite meshes. A material without
those usage flags is drawn with the default material in a packaged build. In the editor it is fixed up on the fly,
with a warning, so it looks right while testing and wrong when shipped. Run after import_rock_models.py,
import_dark_biome.py or create_ocean_materials.py.

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript=<this file> -unattended -nullrhi
"""
import unreal

eal = unreal.EditorAssetLibrary

# The same meshes as ThalassaClutter::MeshPaths in ThalassaOceanClutter.cpp.
MESHES = [
    "/Game/World/Rocks/moon_rock_01/moon_rock_01/StaticMeshes/moon_rock_01_LOD0",
    "/Game/World/Rocks/moon_rock_03/moon_rock_03/StaticMeshes/moon_rock_03",
    "/Game/World/Rocks/moon_rock_05/moon_rock_05/StaticMeshes/moon_rock_05",
    "/Game/World/Rocks/rock_07/rock_07/StaticMeshes/rock_07",
    "/Game/World/Rocks/rock_09/rock_09/StaticMeshes/rock_09",
    "/Game/World/Rocks/namaqualand_boulder_03/namaqualand_boulder_03/StaticMeshes/namaqualand_boulder_03",
    "/Game/World/Rocks/namaqualand_boulder_05/namaqualand_boulder_05/StaticMeshes/namaqualand_boulder_05",
    "/Game/World/Rocks/coast_rocks_05/coast_rocks_05/StaticMeshes/coast_rocks_05",
] + ["/Game/World/DarkBiome/SM_DarkBoulder_%02d" % i for i in range(1, 7)] \
  + ["/Game/World/Formations/SM_SlabTower_%02d" % i for i in range(1, 7)] \
  + ["/Game/World/Formations/SM_SlabFragment_%02d" % i for i in range(1, 6)]

# The scanned rocks' materials are instances of the glTF importer's own materials, which live in the engine, not
# the project. The project never changes engine files: those instances get copies of the importer's base material
# and its in-between instance, kept here, instead.
ENGINE_BASE = "/InterchangeAssets/gltf/M_Default"
OWN_BASE = "/Game/World/Rocks/M_ScanRock"
copies = {}


def own_copy(engine_material):
    """The project's copy of an engine material in the chain, made once."""
    path = engine_material.get_path_name().split(".")[0]
    if path in copies:
        return copies[path]
    target = OWN_BASE if path == ENGINE_BASE else "/Game/World/Rocks/" + path.split("/")[-1].replace("MI_Default", "MI_ScanRock")
    if not eal.does_asset_exist(target):
        if not eal.duplicate_asset(path, target):
            unreal.log_error("THALASSA: could not copy %s to %s" % (path, target))
            return None
        unreal.log("THALASSA: copied %s to %s" % (path, target))
    copy = unreal.load_asset(target)
    if isinstance(copy, unreal.MaterialInstanceConstant):
        parent = copy.get_editor_property("parent")
        if parent and not parent.get_path_name().startswith("/Game/"):
            unreal.MaterialEditingLibrary.set_material_instance_parent(copy, own_copy(parent))
            eal.save_asset(target, only_if_is_dirty=False)
    copies[path] = copy
    return copy


done = set()
reparented = 0
for path in MESHES:
    mesh = unreal.load_asset(path)
    if not mesh:
        unreal.log_error("THALASSA: no mesh at " + path)
        continue
    for slot in range(len(mesh.get_editor_property("static_materials"))):
        material = mesh.get_material(slot)
        parent = material.get_editor_property("parent") if isinstance(material, unreal.MaterialInstanceConstant) else None
        if parent and parent.get_path_name().startswith("/InterchangeAssets/"):
            copy = own_copy(parent)
            if copy:
                unreal.MaterialEditingLibrary.set_material_instance_parent(material, copy)
                eal.save_asset(material.get_path_name(), only_if_is_dirty=False)
                reparented += 1
        base = material.get_base_material() if material else None
        if not base or base.get_path_name() in done:
            continue
        done.add(base.get_path_name())
        if not base.get_path_name().startswith("/Game/"):
            unreal.log_warning("THALASSA: %s uses %s, outside the project; left alone" % (path, base.get_path_name()))
            continue
        changed = False
        for flag in ("used_with_instanced_static_meshes", "used_with_nanite"):
            if not base.get_editor_property(flag):
                base.set_editor_property(flag, True)
                changed = True
        if changed:
            unreal.MaterialEditingLibrary.recompile_material(base)
            eal.save_asset(base.get_path_name())
            unreal.log("THALASSA: usage flags set on " + base.get_path_name())
unreal.log("THALASSA: %d rock materials now use %s; checked %d base materials on %d rock meshes" % (reparented, OWN_BASE, len(done), len(MESHES)))
