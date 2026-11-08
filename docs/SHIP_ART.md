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

## Moving gameplay spots

The places the game cares about are components in `BP_Thalassa`, under **Thalassa | Spots** in its Details panel and in the component list. Select one and move it, and the game follows. You never see them in play, and they never collide.

| Component | What it is | Seen in the editor as |
|---|---|---|
| `DryCompartment00`, `01`, ... | Spaces with air. Inside one you walk and move with the ship; outside all of them is sea and you swim. | Blue boxes |
| `LadderZone00` | Where you take hold of a ladder: about 25 cm deep in front of it, its full width, foot to just above the top. | Yellow box |
| `SpawnPoint00` to `07` | Where players appear when they join, in name order. | Green capsules |
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

## Meshes from Blender

Export as FBX or glTF, import into `Content/Ship/` (or a folder under it), and place as above. Scale is centimetres: 1 Unreal unit is 1 cm. In ship coordinates, +X is forward to the bow, +Y is starboard, and +Z is up. Source art files (`.blend`, `.fbx`) go through Git LFS automatically.

## Not done yet

- **A walkable shell from your own mesh.** Today the walkable collision is the grey layout's boxes. Next, a simple mesh of your own, made of floors, walls, ramps and steps, can become the one collision component instead, so the art and the walking can change together.
- **A check for the other rules,** such as steps too tall or doorways too narrow, run on the walkable shell.

## Tested

- Gameplay spots as components (2026-11-08): with nothing moved, the deck test had 0 corrections, no stuck waypoints and identical sonar charts. The helmsman drove the Blueprint ship to 14 m/s with 0 corrections, and the rig dive completed (18 corrections, all under 10 cm). In a throwaway copy of the Blueprint, moving `SpawnPoint00` to the bridge made the first player start exactly there.

- Standard deck test with the Blueprint spawned instead of the C++ ship: same results as before. One correction of 2 cm, and identical sonar charts on host and client.
- `run_motion_test.py --deck --art-probe` attaches a solid 1.5 m crate to the ship in the corridor the walkers use. On both host and client the log reported it turned into scenery, and the walkers walked through it with no corrections and no stuck waypoints.
