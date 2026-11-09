# Building the ship's look

The ship's behaviour is C++: moving, the helm, the sonar, stations, swimming, networking. Its look is yours to build in the editor. This page says how, and which rules your art must keep.

## Where the ship is

**`Content/Ship/BP_Thalassa`** is the ship. It's a Blueprint of the C++ ship (`AThalassaVessel`), and the game spawns it (`Config/DefaultGame.ini`). You never need to write Blueprint logic in it. It's the place where the ship's meshes, rooms and lights are arranged.

Open it, and the viewport shows the whole grey ship. Anything you add there moves with the ship, appears for every player, and is packaged with the game.

If the Blueprint is ever deleted or fails to load, the game falls back to the plain grey C++ ship and says so in the log. `Scripts/unreal/create_ship_blueprint.py` makes a fresh one, and never overwrites an existing one.

## Show Greybox

In `BP_Thalassa`'s Details panel, under **Thalassa | Art**, **Show Greybox** hides the grey boxes, so you can see your art on its own, in the editor and in play.

Only what you see changes. Walking uses the walkable shell (below), so your floors must sit where the shell's floors are.

## The rule your art must keep

**Everything players walk on is one collision component** (`docs/INTERIOR_RULES.md`, rule 1). If a player stands on a separate piece and the network loses packets, other players see them metres from where they really are. The rule came from a test that failed.

So everything you add is **scenery**: players can neither stand on it nor bump into it. The ship enforces this. When the game starts, every component you added in `BP_Thalassa`, and every actor attached to the ship, has its collision with players switched off. The log says when it changes something:

> THALASSA: BP_Thalassa_C.Crate is scenery on the ship, so players now pass through it.

Line traces still hit your art, so looking at things still works. The ship's own collision checks and the sonar ignore it, so the ship never jams on its own furniture, and the sonar never maps its own hull.

The other interior rules also apply to the walkable shell when it comes: stairs are ramps, steps under 45 cm, doorways at least 2 m by 2.5 m, every walkable space inside a dry compartment, and lights in sealed spaces.

## How to build rooms

The comfortable way, and how Unreal projects usually work:

1. **Build a room in its own level**, in the normal editor: meshes, props and lights, with the room's floor at height 0 and its entrance where the grey doorway is.
2. **Turn it into a Packed Level Actor**: select everything, right-click, then Level > Create Packed Level Actor. That makes a Blueprint holding the whole room, drawn efficiently.
3. **Add the room to `BP_Thalassa`**: add a Child Actor component, set its class to the room, and move it into place over the grey room.
4. **Play.** Then switch Show Greybox off and look for gaps.

Small things, like a single prop or light, can go straight into `BP_Thalassa` as components.

Lights must move with the ship: set them to **Movable**.

### The worked example: the aft corridor

The aft corridor (between engineering and the mess, with the airlock off it) is dressed with the `Hallway_sci_fi` kit from Fab. It's in **`Content/Ship/Art/BP_Art_AftCorridor`**, added to `BP_Thalassa` as the Child Actor component **`Art_AftCorridor`**.

- **What's in it:** one instanced mesh component for each kind of piece: wall panels in three rows, trim, columns, corners, door frames, floor and ceiling tiles, pipes and strip lamps. There are also eight warm point lights.
- **Where things are:** the pieces are placed in ship coordinates, over the grey walls. The kit is 350 cm tall, the same as the deck. Its wall pieces stand with their back at local Y 0 and face +Y, which is the rule the script uses to turn them into the room.
- **Changing it by hand:** open `BP_Art_AftCorridor`, pick a component (say `WallBottom`), and select or move single instances in the viewport.
- **Rebuilding it:** `Scripts/unreal/build_aft_corridor_art.py` makes it all from scratch. It's also the template for dressing another room: change the room's walls and doors at the top, and the Blueprint's name. Running it again replaces any hand changes in that Blueprint.

## Moving gameplay spots

The places the game cares about are components in `BP_Thalassa`, under **Thalassa | Spots** in its Details panel and in the component list. Select one and move it, and the game follows. You never see them in play, and they never collide.

| Component | What it is | Seen in the editor as |
|---|---|---|
| `DryCompartment00`, `01`, ... | Spaces with air. Inside one you walk and move with the ship; outside all of them is sea and you swim. | Blue boxes |
| `LadderZone00` | Where you take hold of a ladder: about 25 cm deep in front of it, its full width, foot to just above the top. | Yellow box |
| `SpawnPoint00` to `07` | Where players appear when they join, in name order. Since 2026-11-08 they're on the bridge's lowest tier, the first a few steps from the helm. | Green capsules |
| `HelmSpot`, `CameraSpot` | Where the person at the helm or the camera station stands. You take a station with E within 2 m of its spot. | Purple capsules |
| `Airlock` | The airlock. Its doors and water hang from it, and its panel and flooding chamber move with it. | A point, with the doors under it |

You can resize and turn the boxes. Screens, the throttle lever, the full stop button and the six hull cameras were already movable in the Blueprint: move them, and they stay put in play.

**Adding more:** add a component and pick **Thalassa Dry Compartment**, **Thalassa Ladder Zone** or **Thalassa Spawn Point**. Extra ones count just like the built-in ones. Built-in ones can't be deleted from the Blueprint, but a dry compartment or ladder zone shrunk to nothing does nothing.

**What does not follow yet:**
- The grey boxes, the collision you walk on, and the airlock's grey walls stay where the C++ layout puts them. Moving a spot doesn't move the floor under it, so keep spots where there is floor. That is the walkable shell's job (below).
- Turning the airlock does nothing yet. Its code assumes the sea is to starboard.
- The hull's outer bounds, used to stop the ship against the seabed, grow to fit dry compartments you move outwards, but never shrink.
- The scripted test walker's route is still numbers in `ThalassaMainDeckLayout.cpp`. Moving spots can strand it, and the test then reports stuck waypoints.

The test rig (`-ThalassaTestRig`) always uses the plain C++ ship, so nothing moved in the Blueprint reaches the automated rig tests.

## The walkable shell

What players walk on is **`Content/Ship/Shell/SM_WalkableShell`**, set as **Walkable Shell** in `BP_Thalassa` (Details panel, **Thalassa | Art**). It's one mesh, so it is the ship's one walkable collision component (`INTERIOR_RULES.md`, rule 1). It's never seen in play: Show Collision in the editor viewport draws it.

Its source is **`Art/Ship/WalkableShell.blend`**. Open it in Blender and you'll find two collections:

- **Collision (UBX boxes: what players walk on).** One box per floor, wall, ramp and step, each its own object named `UBX_SM_WalkableShell_000` and so on. Players walk on exactly these.
- **Shell (reference, not collision).** `SM_WalkableShell`, the same boxes as one solid mesh, so there's something to look at. Unreal needs it, but players never touch it.

It starts as an exact copy of today's grey layout: 179 boxes, ramps included. Every box survived the trip from Blender back into Unreal to within 0.003 cm.

In Blender, 1 unit is 1 m, **+X is forward, -Y is starboard** (Blender mirrors Unreal's Y), and +Z is up.

**To change what players walk on:**

1. In Blender, move, resize, turn, duplicate or delete the `UBX_` boxes. A new box must be named `UBX_SM_WalkableShell_` followed by a new number, and must stay a box: 8 corners, 6 flat faces. Turning it is fine, and that's how the ramps are made. If you like, change the reference mesh too.
2. Export: in Blender's Scripting tab, open `Scripts/blender/walkable_shell_fbx.py` and press Run Script. It writes `Art/Ship/WalkableShell.fbx` next to the `.blend` with the right settings.
3. Import: in the Unreal editor, Tools > Execute Python Script, `Scripts/unreal/import_walkable_shell.py`. It replaces `SM_WalkableShell` and prints how many collision boxes it has. That number should match the number of `UBX_` boxes.
4. Play.

**Why boxes, not the mesh itself:** the first try used the mesh's own triangles as collision. Walkers got stuck climbing out of the sunken mess and the lower bridge tier. Where boxes meet, their hidden inside faces become seams that a capsule catches on when stepping up. Simple boxes don't have that problem, and they're what the grey layout always used.

**Rules still apply:** stairs are ramps (a turned box under 44 degrees), steps under 45 cm, walls down to every floor they enclose, doorways at least 2 m by 2.5 m. The dry compartments (blue boxes, above) must still cover everywhere players can stand.

Starting over from the grey layout: `Scripts/unreal/export_greybox_boxes.py` in Unreal, then `Scripts/blender/make_walkable_shell.py` in Blender. That script never overwrites an existing `.blend` unless you pass `--force`.

Empty the Walkable Shell slot and the ship walks on the grey boxes again.

## Meshes from Blender

Export as FBX or glTF, import into `Content/Ship/` (or a folder under it), and place as above. Scale is centimetres: 1 Unreal unit is 1 cm. In ship coordinates, +X is forward to the bow, +Y is starboard, and +Z is up. Source art files (`.blend`, `.fbx`) go through Git LFS automatically.

## Not done yet

- **Spots that follow the shell.** Moving a floor in Blender doesn't move the spots (dry compartments, spawns, stations) standing on it. Move them in `BP_Thalassa` to match.
- **Reimport from the editor's right-click menu** hasn't been tested: use the import script, which has been.
- **A check for the other rules,** such as steps too tall or doorways too narrow, run on the walkable shell.

## Tested

- Walkable shell (2026-11-08), first on a throwaway copy of `BP_Thalassa` with the shell set (`run_motion_test.py --vessel`). A first version that used the mesh's triangles as collision left walkers stuck 6 times on the mess and bridge steps. The shell imported from Blender, with box collision: deck tour 1 correction of 0.8 cm, no stuck waypoints, identical sonar charts; helm to 14.4 m/s with 0 corrections; at about 167 ms ping, 0 corrections on the client and no stuck waypoints. The Blender round trip came back exact, to 0.003 cm.

- Gameplay spots as components (2026-11-08): with nothing moved, the deck test had 0 corrections, no stuck waypoints and identical sonar charts. The helmsman drove the Blueprint ship to 14 m/s with 0 corrections, and the rig dive completed (18 corrections, all under 10 cm). In a throwaway copy of the Blueprint, moving `SpawnPoint00` to the bridge made the first player start exactly there.

- Standard deck test with the Blueprint spawned instead of the C++ ship: same results as before. One correction of 2 cm, and identical sonar charts on host and client.
- `run_motion_test.py --deck --art-probe` attaches a solid 1.5 m crate to the ship in the corridor the walkers use. On both host and client the log reported it turned into scenery, and the walkers walked through it with no corrections and no stuck waypoints.
