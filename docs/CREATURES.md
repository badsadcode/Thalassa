# Creatures: first version

The first life in the open ocean: schools of big, peaceful gliders, which the ping startles. It is the first of the design document's Phase 2 creatures ("one peaceful, one territorial, one ambiguous distant signal").

## What's there

- **The glider.** A broad-winged animal about 5 m across (4 to 7 m: each animal is scaled 0.8 to 1.35), with a domed dark back, pale lines curving out along the wings, a pale belly, a long whip of a tail, and a faint glowing spot near each wingtip. Placeholder art: `Scripts/blender/make_creatures.py` makes it by rules, `Scripts/unreal/import_creatures.py` imports it as `Content/Creatures/SM_Glider` and builds `M_Creature`.
- **Schools.** 5 to 11 animals, loosely spread through a flattened oval and never nearer each other than 9 m. A school glides 25 to 55 m above the ground, following its shape but not its smallest bumps, rising and falling a little, banking into turns. Each animal drifts slowly within the school and faces a little its own way.
- **Where they live.** The ocean is divided into 600 m fauna cells. The seed decides whether a cell has a school, by province: plains 30%, hills 50%, plateaus 60%, ridges 40%, canyons 50%, volcanic fields 12%. A school always lives near the start, so there's something to meet. Each school wanders two loops added together round its home: a big slow one, 120 to 220 m across, at 1.2 to 2.2 m/s, and a smaller, quicker one, so the path never quite repeats.
- **The ping.** When a ping's wave reaches a school (it spreads at 500 m/s, so a school 1 km away hears it 2 s after it goes out), the school bolts up to 70 m away from where it came from, beating its wings three times as fast, spreading out to over twice as wide, and the wingtip spots flash. Over the next 45 s it settles, gathers and drifts back to its path. Pings close together add up: a school pinged again while still fleeing bolts further. A school gives the ship 80 m of room.
- **Contacts on the chart.** Every animal the ping's wave reaches shows on the sonar chart as an amber diamond with a dark halo, appearing as the wave reaches it, bright at first, fading over 12 s. It marks where the animal was when the wave reached it, not where it is now. A school shows as a cluster.

## How it stays the same for everyone

Nothing is sent over the network for it, like the seabed and the rocks:

- where each school lives, and who is in it, follows from the seed and its cell (`UThalassaOceanFauna::MakeSchool`);
- where a school is at any moment follows from the server's clock, which every machine shares (`SchoolAt`, a pure function);
- how it reacts follows from the replicated ping (each machine keeps the last six pings it has seen).

So every player sees the same animals in the same places, doing the same thing. Contacts are worked out on each machine from the same inputs (`UThalassaSonarComponent::UpdateContacts`). Each machine logs a fingerprint of a ping's contacts, to the metre, so tests can compare them. Two small things can differ: a player who joins late doesn't know about pings sent before, and the room a school gives the ship uses each machine's own view of where the ship is.

## How it's drawn

- One instanced mesh component for all gliders (`Gliders`, on the ocean), with every school within 700 m of the camera, no collision and no shadows. The positions are set every frame on the CPU. The wingbeats are in the material, as a world position offset, so a school costs only its positions.
- **`M_Creature`** reads what each mesh carries (see the top of `make_creatures.py`): the skin from the vertex colours; UV channel 1 where a vertex is on the moving part (out along a wing); channel 2 the animal's size; channel 3 how it moves (0 a body swaying side to side, ready for fish; 1 wings flapping); channel 4 the glow. Per instance, three numbers: phase, wingbeat rate and flash. The beat is about 4 s for a 5 m animal, the tips lagging and travelling furthest.
- **Two things that went wrong, for the record:** vertex alpha doesn't survive the FBX export (every vertex came through as fully glowing, so the glow is in UV channel 4), and FBX colours are read as they are, so they're exported linear, or the dark backs came out pale.

## Switches

- `-ThalassaNoFauna`: no animals, for comparisons.
- `-ThalassaGlidersAt=X,Y` (world centimetres): the start's school lives there, barely roaming. For screenshots: `capture_screenshots.py <folder> /Game/Maps/L_Ocean "close=0/-40@16000,0,-1500" -ThalassaShipAboveGround=40 -ThalassaGlidersAt=17500,0`.
- `-ThalassaPerf` times the fauna's work as "fauna".

## Tested (2026-11-09)

- **Two players on the open ocean** (helm tour, sonar on, a ping every 10 s, about 15 ms): 0 corrections, no frames over 50 ms, the ship in step within 3.7 cm, sonar charts 2,370 and 2,371 cells (one in flight). Host and client found identical contacts, by fingerprint, for pings 6 to 19. Pings 2 to 5 differed: the client joined after ping 1 and never saw it, so schools still fleeing from it were elsewhere on its screen until it was 45 s old. That's the late-joiner limit above.
- **Cost:** the fauna's work is at most about 0.5 ms a frame with schools in view and pinged (`-ThalassaPerf`). One instanced component, about 25 cells looked at per frame.
- **Frames:** a school seen from 40 m (dark backs, glowing wingtips), the same school 4 s after a ping (scattered, fled), and the chart with contacts around the ship.

## Also fixed: the rocks in a packaged build

The game log showed that every scanned rock material lacked the usage flags for instanced meshes and Nanite. In the editor that's fixed up on the fly with a warning. In a packaged build, every rock the open ocean scatters would have been drawn in the default material. The scanned rocks' materials were instances of the glTF importer's own base material, which lives in the engine. `Scripts/unreal/set_clutter_material_usage.py` gives them project copies (`Content/World/Rocks/M_ScanRock` and `MI_ScanRock_Opaque_DS`) with the flags set, and sets them on `M_Rock` too. Run it after reimporting rocks. While writing it, a first version saved the flags into the engine's own `M_Default`. They were set back the same session, but the file isn't byte-identical: verifying the engine in the Epic launcher restores it exactly.

## Not done yet

- **More life:** the owner's "clutter fish": many kinds of small, medium and big fish, giant jellyfish (some glowing), and a leviathan prototype bigger than the ship. `M_Creature` already has the swaying body motion for fish.
- **The territorial creature and the distant signal** from Phase 2.
- **Sound.** The animals are silent, and the hydrophone station doesn't exist yet.
- **Contacts on the holotable,** and classifying them (it's all "something alive" for now).
- **Real art.** The glider is a placeholder of rules, like the grey deck.
- **Creatures that come to look:** at the floodlights, at the ship. These gliders only avoid it.
