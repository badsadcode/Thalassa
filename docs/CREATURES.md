# Creatures

Life in the open ocean: thirteen kinds of animal in schools, from tiny glowing jellyfish to big slow cruisers and krakens, and one leviathan, longer than the ship. The ping startles them, and draws the leviathan. The first of the design document's Phase 2 creatures ("one peaceful, one territorial, one ambiguous distant signal"), and the owner's "clutter fish".

## What's there

| Kind | Size | School | Where | Height above ground | Drawn to | On the ping's chart |
|---|---|---|---|---|---|---|
| Glider: broad wings, glowing wingtip spots | 4 to 7 m across | 5 to 11 | 600 m cells | 25 to 55 m | 700 m | yes |
| Dart: small, slender, silver | 0.3 m | 30 to 80 | 90 m cells | 3 to 15 m | 110 m | no |
| Disc: deep-bodied, gold with dark bands | 0.4 m | 15 to 40 | 140 m | 2 to 8 m | 110 m | no |
| Lantern: small, dark, glowing spots along the belly | 0.2 m | 20 to 60 | 110 m | 8 to 40 m | 160 m | no |
| Grouper: heavy, mottled | 1.4 m | 2 to 6 | 160 m | 0.8 to 3 m | 150 m | no |
| Eel: long, ribbon fins | 2.2 m | 1 to 3 | 200 m | 0.4 to 1.5 m | 110 m | no |
| Cruiser: big, slow, long-finned, a faint glowing line along each flank | 7 m | 1 to 3 | 500 m | 15 to 60 m | 450 m | yes |
| Giant jellyfish: pale, clear or deep red, some with a faintly glowing rim | 6 m bell, tentacles to 14 m | 1 to 3 | 400 m | 25 to 90 m | 400 m | yes |
| Glow jellyfish: glowing rim and banded tentacles, in one of six colours | 2.2 m bell | 3 to 9 | 250 m | 12 to 60 m | 350 m | no |
| Motes: tiny glowing jellyfish, drifting swarms, in one of six colours | 0.3 m | 40 to 110 | 150 m | 5 to 50 m | 160 m | no |
| Helmet jellyfish: a tall bell, deep red or dusky purple, or with a glowing amber or violet ring | 1.6 m tall | 4 to 14 | 300 m | 10 to 70 m | 300 m | no |
| Comb jellyfish: egg-shaped, eight glowing rows, in one of six colours | 1.2 m | 6 to 20 | 200 m | 5 to 40 m | 220 m | no |
| Kraken: a giant squid-like animal, dark red-brown with pale spots that glow faintly, big glowing eyes | 55 to 90 m | 1 | 2.5 km cells | 50 to 140 m | 2.2 km | yes, one big contact |
| Elder kraken: the owner's own model, textured, a mean head and long suckered limbs; comes closer when pinged | 140 to 220 m | 1 | 6 km cells | 80 to 200 m | 3 km | yes, one huge contact |

Each animal is scaled about 0.8 to 1.3 times. Each kind has its own lattice of cells, and the seed decides whether a cell holds a school of that kind, by province (the table is `ThalassaFauna::Species` in `ThalassaOceanFauna.cpp`). Small fish are everywhere; lanterns and glowing jellyfish like the dark canyons and volcanic fields; groupers and eels the broken ground; the plains are thinner. Every kind has a school near the start, so there's something to meet.

- **Schools** wander two loops added together round their home, a big slow one and a smaller quicker one, so the path never quite repeats, at a height over the ground, following its shape (closely for fish near the ground), banking into turns. Each animal drifts within the school and faces a little its own way. Jellyfish drift slowly, upright, turning and tilting a little.
- **Motion in the materials:** fish bodies sway with a wave running back to the tail (small fish fast, big ones slowly); gliders' wings beat; jellyfish bells pulse and their tentacles trail; a kraken's mantle draws in, and its arms close together, spread and undulate.
- **Jellyfish swim in strokes.** Each bell draws in and relaxes (every 1.5 s for a mote, about 3 s for a giant: the period is 1.3 times the square root of its size), and with each stroke the jellyfish bobs up and sinks back, the stroke rippling down its tentacles. Its glow follows the stroke: brightest as the bell draws in, dimming as it relaxes. Each has its own rhythm, so a swarm shimmers rather than blinking together. The stroke is worked out from the server's clock (and handed to the material as the phase), so the bob and the pulse match, and every player sees the same stroke.
- **Colours.** Jellyfish meshes are pale; each school picks a colour from its kind's list (the palettes in `ThalassaOceanFauna.cpp`), and whether it glows: glowing blue, violet, green, amber, pink or cyan; or not glowing at all, pale, nearly clear, deep red or dusky purple, which is how real deep-sea jellyfish hide (red looks black where no red light reaches). Each animal is a little lighter or darker than its school.
- **The elder kraken** is the owner's model (`Art/Creatures/Kraken_Test.blend`: a body, eight limbs, two eye spheres, about 30,000 triangles each, textures packed in). `Scripts/blender/make_kraken2.py` turns it into `SM_KrakenElder`: limbs cut to about 6,000 triangles and the body to 18,000 (69,000 in all), joined, turned head first, 100 m long at scale 1, with the data the creature motion reads (body 0 to 0.38, each limb 0.4 at its root to 1 at its tip, kraken motion), the orange iris mapped across the front of each eye sphere, and its textures saved beside it. `import_creatures.py` imports it into `Content/Creatures/KrakenElder` with `M_CreatureTextured` (the same motion, colour and normal from textures). It doesn't flee a ping: it comes 150 m closer, facing it. One lives near the start; elsewhere about one in 6 km cells, mostly in canyons.
- **Krakens** have the owner's orange iris with a horizontal slit, as real squid do. They swim mantle first, in slow strokes (about 10 s), surging forward with each one. A ping makes one bolt 250 m and its pale spots flare. One lives near the start; elsewhere, mostly in canyons, ridges and volcanic fields.
- **The ping:** when its wave reaches a school (500 m/s), the school bolts away from where it came from, swims faster, spreads out, and its glow flashes; over 45 s it settles and drifts back. Jellyfish drift only a little, and their glow flares for a few seconds. Pings close together add up. Every school gives the ship room (10 to 80 m, by kind).
- **Contacts on the chart:** everything big enough (gliders, cruisers, giant jellyfish, the leviathan) shows as an amber diamond with a dark halo, appearing as the wave reaches it and fading over 12 s, where it was when the wave reached it. Small fish are too small for the ping.

## The leviathan

One, about 400 m long (the ship is 280 m), a body of 16 segments tapering to a tail with an upright fluke, a ridge of spines, trailing fins, rows of glowing spots along its flanks, and two big glowing amber eyes over 4 m across: what you'd see through a window, with the rest of it lost in the dark.

- **Its round:** a big slow loop, 0.9 to 1.2 km across, round a point about 1.6 km from the start, at 7 to 9 m/s, 120 m above the ground, never nearer the surface than 60 m. For this seed the loop is centred at about (-390, 1550) m.
- **Pings draw it.** It hears a ping from up to 2.2 km. Over the next two and a half minutes it comes to circle where the ping came from, about 450 m out and a little below, and after five minutes or so it loses interest and goes back to its round. Pings close together hold its interest. Its lights flare when a ping's wave passes over it.
- **On the chart** it's a long chain of contacts, one from each part of its body: the design document's "multiple apparently separate sonar returns from one body".
- **Its eyes** carry the owner's iris texture: a green, upright slit, glowing faintly from within (`M_CreatureEye`, the meshes' second material slot).
- **How its body moves:** the head's path is a function of time like everything else; each piece of the body is placed along the path the head has taken, sampled every half second and cached, with a slow wave running back along it. So the body follows the head through every turn, and every machine draws it the same.
- **It passes through the ship.** It doesn't avoid it yet: keeping clear would need the ship's position, which each machine sees a little differently. Its round keeps it at least several hundred metres from the start, and when it comes to a ping it circles 450 m out.

## How it stays the same for everyone

Nothing is sent over the network but the pings:

- where each school lives, and who is in it, follows from the seed and its cell (`UThalassaOceanFauna::MakeSchool`);
- where a school is at any moment follows from the server's clock, which every machine shares (`SchoolAt`, a pure function), and the same for the leviathan (`LeviathanHeadAt`);
- how they react follows from the sonar's replicated list of the last ten minutes of pings (`UThalassaSonarComponent::GetRecentPulses`), so a player who joins late sees them where everyone else does.

Contacts are worked out on each machine from the same inputs (`UThalassaSonarComponent::UpdateContacts`), with the ship taken to be where the ping went out. Each machine logs a fingerprint of a ping's contacts, to the metre, so tests can compare them. One small thing can differ: the room a school gives the ship on screen uses each machine's own view of where the ship is.

## How it's drawn

- One instanced mesh component per mesh (16: thirteen kinds and the leviathan's head, body segment and tail), on the ocean, with no collision and no shadows. The positions are set every frame on the CPU; the motion is in the materials (world position offset).
- **The meshes** are placeholders made by rules: `Scripts/blender/make_creatures.py` makes them, `Scripts/unreal/import_creatures.py` imports them into `Content/Creatures` and builds the materials. **`M_Creature`** (lit) draws fish, gliders and the leviathan; **`M_Jelly`** (see-through, unlit, brighter and more solid at the edges) the jellyfish. Both read what each mesh carries (see the top of `make_creatures.py`): skin from the vertex colours; UV channel 1 where a vertex is on the moving part; channel 2 its size; channel 3 how it moves (body, wings, jellyfish, not at all, or kraken); channel 4 its glow. A bright skin glows its own colour, deepened (the leviathan's amber eyes, the jellyfish), a dark one cold blue-green. Per instance, seven numbers: phase (for jellyfish and krakens, the stroke itself), rate, flash, tint, and how much it glows.
- **Two things that went wrong, for the record:** vertex alpha doesn't survive the FBX export (every vertex came through fully glowing, so the glow is in UV channel 4), and FBX colours are read as they are, so they're exported linear, or the dark backs came out pale.

## Switches

- `-ThalassaAmbient=<intensity>`: a test of the light. The ambient (sky) light gets an even source, the water's colour from every direction, instead of its capture of the dark water. Why so much looks black: the only light in the deep is a faint light from above and an ambient light that comes only from above, with the lower half black. So anything facing sideways or down (a creature's flank, an arch's sides, a kraken's arms) gets almost nothing, and shows as a dark shape against the bright water. Turning that ambient up does nothing, because it multiplies a capture of dark water. With the even source, 0.3 shows forms and textures while keeping the murk; 1.0 is flat and milky. Not the default yet: the owner's call.

- `-ThalassaNoFauna`: no animals, for comparisons.
- `-ThalassaFaunaAt=X,Y` (world centimetres): every kind's school from the start gathers round there, barely roaming. For screenshots: `capture_screenshots.py <folder> /Game/Maps/L_Ocean "among=137/-15@16000,0,-7400" -ThalassaShipAboveGround=40 -ThalassaFaunaAt=16000,0` (the ground there is about 55 m below the ship's start).
- `-ThalassaGlidersAt=X,Y`: the same for the gliders alone.
- `-ThalassaLeviathanAt=X,Y`: the leviathan's round is a small loop (250 m) round there.
- `-ThalassaPerf` times the fauna's work as "fauna". Every 20 s the log has a `LEVIATHAN` line: time, head position, and distance from the ship.

## Tested (2026-11-09)

- **Two players on the open ocean, gliders only** (the first version; helm tour, sonar on, a ping every 10 s, about 15 ms): 0 corrections, no frames over 50 ms. Host and client found identical contacts for pings 6 to 19; pings 2 to 5 differed, because the client joined after ping 1 and didn't know about it. That's why the recent pings are now replicated.
- **Two players, all ten kinds and the leviathan** (the same run): 0 corrections, no frames over 50 ms, the ship in step within 2.9 cm, sonar charts 2,370 and 2,371 cells (one in flight). The client joined after pings 1 and 2, and its contacts still matched the host's, by fingerprint, for every ping both saw (3 to 19), including those in the 45 s after the pings it missed. 80 to 110 contacts a ping. The leviathan heard the first pings from 2.2 km and came in: 2,174 m from the ship at 20 s, 1,065 m at 80 s, about 700 m from 120 s while the ship toured away from where it had pinged. This run had the leviathan at its first size (about 155 m); it was scaled to 400 m afterwards, which changes where its pieces are drawn but not how it moves.
- **Two players, with the pulsing, coloured jellyfish and the krakens** (the same run again): 1 correction of 0.0 cm, no frames over 50 ms, the ship in step within 1.8 cm, identical sonar charts (2,367 cells, same checksum). Contacts identical, by fingerprint, for every ping both saw (3 to 19; the client joined after ping 2).
- **Cost:** the fauna's work is at most about 0.85 ms a frame with all the kinds near the start in view, and 0.75 ms with the leviathan in view (`-ThalassaPerf`). With thirteen kinds and a kraken gathered round the camera: about 0.9 ms, the worst frame 6.7 ms, about 225 frames a second. The leviathan's path is cached, so it costs one new sample every half second.
- **Frames:** each kind gathered round one spot (`-ThalassaFaunaAt`); the leviathan at 500 m (a line of lights in the dark) and at 300 m (its head rising, eye, spine ridge, rows of lights); the chart, with the leviathan as a curved chain of contacts.

## Also fixed: the rocks in a packaged build

The game log showed that every scanned rock material lacked the usage flags for instanced meshes and Nanite. In the editor that's fixed up on the fly with a warning. In a packaged build, every rock the open ocean scatters would have been drawn in the default material. The scanned rocks' materials were instances of the glTF importer's own base material, which lives in the engine. `Scripts/unreal/set_clutter_material_usage.py` gives them project copies (`Content/World/Rocks/M_ScanRock` and `MI_ScanRock_Opaque_DS`) with the flags set, and sets them on `M_Rock` too. Run it after reimporting rocks. While writing it, a first version saved the flags into the engine's own `M_Default`. They were set back the same session, but the file isn't byte-identical: verifying the engine in the Epic launcher restores it exactly.

## Not done yet

- **Visibility.** Small fish are true to size, and in the murk they're dark specks beyond 20 or 30 m. Near the floodlights they read; further out, only the glowing kinds do. That may be right for the deep, but the sunlit and twilight water from Phase 2 will show them better.
- **The territorial creature and the distant signal** from Phase 2. The leviathan is curious, not territorial.
- **Sound.** The animals are silent, and the hydrophone station doesn't exist yet. The leviathan should be heard long before it's seen.
- **The leviathan and the ship:** it doesn't avoid the ship, and doesn't come to the windows. A pass by the observation window, with the eye, needs it to know where the ship is.
- **Contacts on the holotable,** and classifying them (it's all "something alive" for now).
- **Real art.** Every creature is a placeholder of rules, like the grey deck.
- **Creatures that come to look** at the floodlights or the ship. Schools only avoid it.
