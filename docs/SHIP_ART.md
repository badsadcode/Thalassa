# Building the ship's look

The ship's behaviour is C++: moving, the helm, the sonar, stations, swimming, networking. Its look is yours to build in the editor. This page says how, and which rules your art must keep.

## Where the ship is

**`Content/Ship/BP_Thalassa`** is the ship. It's a Blueprint of the C++ ship (`AThalassaVessel`), and the game spawns it (`Config/DefaultGame.ini`). You never need to write Blueprint logic in it. It's the place where the ship's meshes, rooms and lights are arranged.

Open it, and the viewport shows the whole grey ship. Anything you add there moves with the ship, appears for every player, and is packaged with the game.

If the Blueprint is ever deleted or fails to load, the game falls back to the plain grey C++ ship and says so in the log. `Scripts/unreal/create_ship_blueprint.py` makes a fresh one, and never overwrites an existing one.

## Show Greybox

In `BP_Thalassa`'s Details panel, under **Thalassa | Art**, **Show Greybox** hides the grey boxes, so you can see your art on its own, in the editor and in play.

Only what you see changes. Walking still uses the grey layout's collision. So until a walkable shell exists (see below), your floors must sit where the grey floors are.

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

## Meshes from Blender

Export as FBX or glTF, import into `Content/Ship/` (or a folder under it), and place as above. Scale is centimetres: 1 Unreal unit is 1 cm. In ship coordinates, +X is forward to the bow, +Y is starboard, and +Z is up. Source art files (`.blend`, `.fbx`) go through Git LFS automatically.

## Not done yet

- **A walkable shell from your own mesh.** Today the walkable collision is the grey layout's boxes. Next, a simple mesh of your own, made of floors, walls, ramps and steps, can become the one collision component instead, so the art and the walking can change together.
- **Gameplay spots as markers you drag.** Station spots, screens, ladder climbing zones, dry compartments, the airlock, spawn points and hull cameras are still numbers in `ThalassaMainDeckLayout.cpp`. They should become components you move in `BP_Thalassa`.
- **A check for the other rules,** such as steps too tall or doorways too narrow, run on the walkable shell.

## Tested

- Standard deck test with the Blueprint spawned instead of the C++ ship: same results as before. One correction of 2 cm, and identical sonar charts on host and client.
- `run_motion_test.py --deck --art-probe` attaches a solid 1.5 m crate to the ship in the corridor the walkers use. On both host and client the log reported it turned into scenery, and the walkers walked through it with no corrections and no stuck waypoints.
