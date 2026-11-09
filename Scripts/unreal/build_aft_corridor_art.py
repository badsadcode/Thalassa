"""Dresses the aft corridor with the Hallway_sci_fi kit: a worked example of the ship's art over its grey box.

Makes /Game/Ship/Art/BP_Art_AftCorridor, a Blueprint holding the room's pieces (one instanced mesh per kind of
piece: walls, columns, door frames, floor, ceiling, trim, lamps, and a few lights), placed in ship coordinates
over the grey room, and adds it to BP_Thalassa as a Child Actor component called Art_AftCorridor.

The room (ThalassaMainDeckLayout.cpp, "Aft: spine"): X -8500 to -6000, between the walls at Y -800 and Y 500,
floor at 0, ceiling at 350 (the kit's own height). Doors: to the stores (south, X -7000, 2 m), to the EVA room
and airlock (north, X -7000, 4 m), to engineering (aft, Y -300, 4 m) and forward (Y -300, 4 m).

Art is scenery: the ship turns its collision off (docs/SHIP_ART.md). What you walk on and bump into is still
the grey layout's, so the art keeps to its walls.

Safe to run again: it rebuilds the room Blueprint and adds the Child Actor only if it isn't there yet.
Open BP_Art_AftCorridor afterwards to move pieces by hand; running this again replaces that work.

Run with: UnrealEditor-Cmd.exe Thalassa.uproject -ExecutePythonScript=<this file> -unattended -nullrhi
"""
import math
import unreal

KIT = "/Game/Hallway_sci_fi/mesh/"
ROOM_BP = "/Game/Ship/Art/BP_Art_AftCorridor"
SHIP_BP = "/Game/Ship/BP_Thalassa"
CHILD_NAME = "Art_AftCorridor"

X0, X1 = -8500.0, -6000.0     # Aft and forward walls.
Y0, Y1 = -800.0, 500.0        # South (stores) and north (EVA room) walls.
HALF_WALL = 15.0              # Grey walls are 30 cm thick: art sits on their faces.
CEILING = 350.0

eal = unreal.EditorAssetLibrary
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary


def mesh(path):
    m = unreal.load_asset(KIT + path)
    if not m:
        raise RuntimeError("THALASSA: kit mesh missing: " + path)
    return m


MESHES = {
    "WallBottom": mesh("a1_mesh/wall_bot_a1/SM_wall_bot_a1"),
    "WallMiddle": mesh("a1_mesh/wall_mid_a1/SM_wall_mid_a1"),
    "WallTop": mesh("a1_mesh/wall_top_a1/SM_wall_top_a1"),
    "Trim": mesh("a1_mesh/decel_a1/SM_decel_a1"),
    "Column": mesh("a1_mesh/column_a1_1/SM_column_a1_1"),
    "Corner": mesh("a1_mesh/coner_a1/SM_coner_a1"),
    "Floor": mesh("a1_mesh/floor_a1_2_2/SM_floor_a1_2_2"),
    "Ceiling": mesh("a1_mesh/celler_a1_2_2/SM_celling_a1_2_2"),
    "DoorSmall": mesh("mesh_doors/door1/SM_door_1"),
    "DoorLarge": mesh("mesh_doors/door2/SM_door2frame"),
    "Lamp": mesh("lamp/SM_lamp_1"),
    "Pipe": mesh("pipe/SM_p_6"),
}
placed = {name: [] for name in MESHES}


def put(name, x, y, z, yaw=0.0, sx=1.0, sy=1.0, sz=1.0, roll=0.0):
    placed[name].append(unreal.Transform(unreal.Vector(x, y, z), unreal.Rotator(roll, 0.0, yaw), unreal.Vector(sx, sy, sz)))


# A wall: a line, the direction along it, and the way into the room. Kit pieces stand with their back at local
# y = 0 and their face towards +y, x across the piece.
class Wall:
    def __init__(self, start, end, inward):
        self.start, self.end, self.inward = start, end, inward
        dx, dy = end[0] - start[0], end[1] - start[1]
        self.length = math.hypot(dx, dy)
        self.along = (dx / self.length, dy / self.length)
        # Yaw that turns local +y into the room.
        self.yaw = math.degrees(math.atan2(-inward[0], inward[1]))

    def point(self, s, depth=0.0):
        """s cm along the wall, depth cm out from the grey wall's face into the room."""
        d = HALF_WALL + depth
        return (self.start[0] + self.along[0] * s + self.inward[0] * d, self.start[1] + self.along[1] * s + self.inward[1] * d)

    def local_x_sign(self):
        """+1 if the piece's local +x runs the same way as the wall."""
        r = math.radians(self.yaw)
        return 1.0 if (math.cos(r) * self.along[0] + math.sin(r) * self.along[1]) > 0 else -1.0


def panels(wall, a, b, column_every=3):
    """Wall panels between a and b cm along the wall, stretched to fit whole panels, and a column on every
    third seam."""
    n = max(1, round((b - a) / 200.0))
    width = (b - a) / n
    for i in range(n):
        x, y = wall.point(a + width * (i + 0.5))
        sx = width / 200.0
        put("WallBottom", x, y, 0.0, wall.yaw, sx)
        put("WallMiddle", x, y, 100.0, wall.yaw, sx)
        put("WallTop", x, y, 200.0, wall.yaw, sx)
        put("Trim", x, y, 300.0, wall.yaw, sx)
        if i > 0 and i % column_every == 0:
            cx, cy = wall.point(a + width * i)
            put("Column", cx, cy, 0.0, wall.yaw)
    # Pipes along the top of the wall, under the trim.
    s = a
    while s + 200.0 <= b + 1.0:
        # The pipe runs from local x 0 to 200: turn it to run along the wall.
        px, py = wall.point(s, 60.0)
        put("Pipe", px, py, 290.0, wall.yaw + (0.0 if wall.local_x_sign() > 0 else 180.0))
        s += 200.0


def door(wall, centre, opening):
    """A frame round a doorway: the small kit door (2 m wide as it comes, its opening 1.6 m) or the large one
    (6.5 m, its opening 5.3 m), scaled to the grey doorway. Returns the frame's half width."""
    x, y = wall.point(centre)
    if opening <= 250.0:
        sx = opening / 160.0
        # Its frame runs from local x 0 to 200, and sits from local y -109 to 9: centre it on the wall.
        ox = -100.0 * sx * wall.local_x_sign()
        put("DoorSmall", x + wall.along[0] * ox + wall.inward[0] * (50.0 - HALF_WALL),
            y + wall.along[1] * ox + wall.inward[1] * (50.0 - HALF_WALL), 0.0, wall.yaw, sx)
        return 100.0 * sx
    sx = opening / 530.0
    put("DoorLarge", *wall.point(centre, -HALF_WALL), 0.0, wall.yaw, sx)
    return 325.0 * sx


def dress_wall(wall, doors):
    """Corners take the first and last metre; panels fill the rest round the doors."""
    cursor = 100.0
    for centre, opening in sorted(doors):
        half = door(wall, centre, opening)
        panels(wall, cursor, centre - half)
        cursor = centre + half
    panels(wall, cursor, wall.length - 100.0)


south = Wall((X0, Y0), (X1, Y0), (0.0, 1.0))
north = Wall((X1, Y1), (X0, Y1), (0.0, -1.0))
aft = Wall((X0, Y1), (X0, Y0), (1.0, 0.0))
fore = Wall((X1, Y0), (X1, Y1), (-1.0, 0.0))
dress_wall(south, [(-7000.0 - X0, 200.0)])
dress_wall(north, [(X1 - (-7000.0), 400.0)])
dress_wall(aft, [(Y1 - (-300.0), 400.0)])
dress_wall(fore, [(-300.0 - Y0, 400.0)])

# Corners: the kit's corner piece fills a metre square, its back to both walls.
for (cx, cy), (ix, iy) in [((X0, Y0), (1, 1)), ((X1, Y0), (-1, 1)), ((X1, Y1), (-1, -1)), ((X0, Y1), (1, -1))]:
    x, y = cx + ix * HALF_WALL, cy + iy * HALF_WALL
    # The piece spans local x -100..0 and y 0..100: turn local +y into the room from one wall, and mirror
    # it if local -x doesn't then run into the room from the other.
    for yaw in (0.0, 90.0, 180.0, -90.0):
        r = math.radians(yaw)
        ly = (-math.sin(r), math.cos(r))
        lx = (math.cos(r), math.sin(r))
        if round(ly[1]) == iy and round(ly[0]) == 0:
            mirror = 1.0 if round(-lx[0]) == ix else -1.0
            put("Corner", x, y, 0.0, yaw, mirror)
            break

# Floor and ceiling: tiles stretched to fit the room exactly. Floor tiles span local x -100..100, y 0..200.
inner_x0, inner_x1 = X0 + HALF_WALL, X1 - HALF_WALL
inner_y0, inner_y1 = Y0 + HALF_WALL, Y1 - HALF_WALL
nx = max(1, round((inner_x1 - inner_x0) / 200.0))
ny = max(1, round((inner_y1 - inner_y0) / 200.0))
tx, ty = (inner_x1 - inner_x0) / nx, (inner_y1 - inner_y0) / ny
for i in range(nx):
    for j in range(ny):
        x = inner_x0 + tx * (i + 0.5)
        y = inner_y0 + ty * j
        put("Floor", x, y, 0.3, 0.0, tx / 200.0, ty / 200.0)
        # The ceiling tile spans local y 0..201 and z -10..0.
        put("Ceiling", x, y, CEILING - 1.0, 0.0, tx / 200.0, ty / 201.0)

# Strip lamps on the ceiling, in two rows down the room, with real lights among them.
lights = []
for i, x in enumerate([-8150.0, -7550.0, -6950.0, -6350.0]):
    for y in (-450.0, 150.0):
        put("Lamp", x, y, CEILING - 12.0, 0.0, roll=180.0)
        lights.append((x, y))


# ---------------------------------------------------------------- The Blueprint.
if eal.does_asset_exist(ROOM_BP):
    # The ship refers to it, so it can't be deleted: empty it instead.
    room = unreal.load_asset(ROOM_BP)
    handles = sds.k2_gather_subobject_data_for_blueprint(room)
    root = handles[0]
    old = [h for h in handles[1:] if lib.is_handle_valid(h)]
    if old:
        sds.delete_subobjects(root, old, room)
else:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.Actor)
    room = unreal.AssetToolsHelpers.get_asset_tools().create_asset("BP_Art_AftCorridor", "/Game/Ship/Art", unreal.Blueprint, factory)
    root = sds.k2_gather_subobject_data_for_blueprint(room)[0]


def add(cls, name, parent=root):
    handle, fail = sds.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent, new_class=cls, blueprint_context=room))
    if not lib.is_handle_valid(handle):
        raise RuntimeError("THALASSA: could not add %s: %s" % (name, fail))
    sds.rename_subobject(handle, unreal.Text(name))
    return lib.get_object(lib.get_data(handle))


total = 0
for name, transforms in placed.items():
    if not transforms:
        continue
    ism = add(unreal.InstancedStaticMeshComponent, name)
    ism.set_editor_property("static_mesh", MESHES[name])
    ism.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    for t in transforms:
        ism.add_instance(t)
    total += len(transforms)
    unreal.log("THALASSA: %s x%d" % (name, len(transforms)))

for index, (x, y) in enumerate(lights):
    light = add(unreal.PointLightComponent, "Light%02d" % index)
    light.set_editor_property("relative_location", unreal.Vector(x, y, CEILING - 30.0))
    light.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    light.set_editor_property("intensity", 14.0)
    light.set_editor_property("attenuation_radius", 900.0)
    light.set_editor_property("light_color", unreal.Color(255, 222, 190, 255))
    light.set_editor_property("source_length", 180.0)

unreal.BlueprintEditorLibrary.compile_blueprint(room)
eal.save_asset(ROOM_BP)
unreal.log("THALASSA: %s built, %d pieces, %d lights" % (ROOM_BP, total, len(lights)))

# ---------------------------------------------------------------- On the ship.
ship = unreal.load_asset(SHIP_BP)
ship_handles = sds.k2_gather_subobject_data_for_blueprint(ship)
names = {}
for h in ship_handles:
    obj = lib.get_object(lib.get_data(h))
    if obj:
        names[obj.get_name().replace("_GEN_VARIABLE", "")] = h
if CHILD_NAME in names:
    unreal.log("THALASSA: BP_Thalassa already has " + CHILD_NAME)
else:
    parent = names.get("ShipRoot", ship_handles[0])
    handle, fail = sds.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent, new_class=unreal.ChildActorComponent, blueprint_context=ship))
    if not lib.is_handle_valid(handle):
        raise RuntimeError("THALASSA: could not add the room to the ship: %s" % fail)
    sds.rename_subobject(handle, unreal.Text(CHILD_NAME))
    child = lib.get_object(lib.get_data(handle))
    child.set_editor_property("child_actor_class", unreal.EditorAssetLibrary.load_blueprint_class(ROOM_BP))
    unreal.log("THALASSA: added %s to BP_Thalassa" % CHILD_NAME)
unreal.BlueprintEditorLibrary.compile_blueprint(ship)
eal.save_asset(SHIP_BP)
