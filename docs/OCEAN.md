# The open ocean

The owner's direction (2026-11-08): the world must be far bigger than the 6 km test region, generated procedurally. Terrain comes first, and hand-made assets only once the terrain looks believable. The plan and the lessons from the owner's Thornwake generator are in `SEABED.md` ("A much bigger ocean"). This page is what exists.

## What exists: first version

- **`Content/Maps/L_Ocean`**, made by `Scripts/unreal/create_ocean_map.py`: the shared water look (`Scripts/unreal/thalassa_water.py`) and one `AThalassaOcean` actor. There is no baked seabed. The old test map `L_ShipMotion` is untouched, and every existing test still uses it.
- **`FThalassaSeabedGenerator`** (`Source/Thalassa/World/ThalassaSeabedGenerator.*`): the seabed as a pure function of a seed and a point, in doubles. **Revision 2** (2026-11-08), after the owner found the first version deep and samey:
  - **Basins:** abyssal plains around 2,100 m, rising to shelves around 450 m, over about 160 km. **Around the start there is always a shelf**, so a new crew can reach the bottom. With the default seed the start sits on a plateau 188 m deep, and the abyss lies beyond.
  - **Provinces:** the ocean is divided into jittered 40 km cells, and each picks a character from its hash. Neighbours blend over about 5 km. The six characters:
    - **plains:** low relief, the silent plains;
    - **hills:** up to about 260 m;
    - **plateaus:** the ground breaks into terraces about 90 m high, with steep risers;
    - **ridges:** local ridgelines up to 350 m;
    - **canyons:** up to 700 m deep, with tributaries;
    - **volcanic:** fields of 120 to 600 m cones on a 5 km lattice, some with summit craters.
  - **Everywhere:** mid-ocean ridges over 26 km, rarer on shelves; seamounts on a 24 km lattice, in about half the cells, up to 2.2 km tall; and canyon tributaries.
  - **At floodlight scale:** craggy outcrops (ridged noise over 140 m), strong in rugged provinces and absent on plains, then fine detail and 30 m grit.
  - **A soft cap,** so nothing comes within 60 m of the surface.
  - At start the ocean logs the nearest example of each province. Default seed: plateaus at the start (196 m), ridges at (14.5, 3.9) km (155 m), hills at (−3.9, 19.6) km (368 m), plains at (17.7, −17.7) km (808 m), canyons at (69.6, 7.8) km (724 m), volcanic at (−59.3, −53.7) km (1,548 m).
- **Determinism:** the noise uses integer hashing (SplitMix64) and polynomial smoothing only, with no `sin`, `cos`, `exp` or `pow`. Every machine builds the identical seabed from the seed. `Revision` is bumped whenever the output changes. At start, the ocean logs a fingerprint of heights on a 400 km grid. Seed 20261108, revision 2 gives **5717371967925676395**. (Revision 1 gave 609611599508488193.)
- **`AThalassaOcean`** (`Source/Thalassa/World/ThalassaOcean.*`) streams it, with nothing replicated:
  - **What you see:** nested rings around the local camera. There are five levels, from 256 m tiles up to 4 km tiles. The coarsest level covers ±2 tiles, about 20 km, and each finer level replaces the coarser tiles within one tile of the viewer. Tiles are 64 × 64 quads (4 m apart at level 0), and distant tiles leave out wavelengths shorter than twice their spacing. Skirts hide seams between levels. Tiles are built on worker threads, at most 12 at once, nearest level first, and added within 4 ms per frame. Old tiles stay until all their replacements exist. Each tile sits at its own corner, so its vertices stay small numbers however far out it is.
  - **What you collide with:** full-detail collision tiles, built on every machine around its own player. The host also builds them around every pawn (768 m squares) and the ship (a 1.28 km square). On the first frame they're built at once, so nothing starts over empty space.
- **The sonar** traces the world as before, then follows each beam through the height function (`AThalassaOcean::Raycast`) and keeps the nearer return. It charts generated seabed far beyond any loaded tile. Its range went from 800 m to **3 km**, like a real survey sonar: the open ocean is often over 1 km deep, and at 800 m the first tests charted nothing. The raycast takes long steps far above the ground and short ones close to it, so a 3 km beam costs a few dozen samples.
- **The ship's start:** in the open ocean, the game mode starts the ship 120 m above the seabed at the start point, no shallower than 200 m unless the ground forces it, and never within 30 m of the surface. With the default seed, the seabed is 946 m down at the start, so the ship starts at 200 m, far above anything the floodlights can reach. The log names the shallowest and deepest ground within 20 km: 704 m and 2,385 m.
- **A settling contact counts once:** a new collision counts only if the last was more than 1.5 s ago. Thrusters pushing the ship onto the seabed made it touch three times in 0.7 s.
- **Test boost:** B at the helm, in test and development builds only, gives ×10 top speed (150 m/s ahead) and ×10 on the vertical thrusters, in about the same time to speed. The helm screen shows TEST BOOST. See `HELM.md`.
- **Tools:** `run_motion_test.py --map /Game/Maps/L_Ocean`, `--above-ground <m>` (starts the ship that far above the highest ground under its hull) and `--helm-course sink` (thrusters full down into the seabed) and `capture_screenshots.py ... /Game/Maps/L_Ocean` play the ocean. From Git Bash, set `MSYS_NO_PATHCONV=1`, or `/Game/...` gets rewritten into a Windows path.

## Clutter (2026-11-09)

`UThalassaOceanClutter` (`Source/Thalassa/World/ThalassaOceanClutter.*`), a component of the ocean, places rocks by rules from the seed. Nothing is sent over the network: every machine places the same rocks.

- **Layers, each on its own world-aligned lattice.** A cell's hash decides whether something is there, where, how big, and which mesh. An object's identity is its cell, so nothing moves or pops when you come back.
  - Stones, 0.3 to 2 m, per 8 m cell, drawn to 250 m.
  - Boulders, 1.5 to 7.5 m (mostly small), per 32 m cell, often with a few broken pieces around them; drawn to 800 m.
  - Rock masses, 10 to 28 m, per 128 m cell; drawn to 2 km.
  - Slab tower clusters (one to three towers, fragments, and boulders at their feet), per 256 m cell, on level ground only; drawn to 2.5 km.
- **Rules.** Each province has its own density per layer: the plains stay almost empty, ridges and volcanic fields are strewn with rock, plateaus get most of the towers. Stones and boulders gather on slopes (scree), up to about 3.5 times more, but none on cliffs over about 55 degrees. A 128 m clumping field makes some stretches strewn and others nearly bare. Province borders are dithered, so they never show as a line. Volcanic ground uses the dark fractured boulders; elsewhere, the Poly Haven scans.
- **Settling.** Each rock is sunk to the lowest ground under it, by 15% of its size (20% for masses), and leans halfway with the slope, with a little random tilt.
- **Solid or not.** Boulders of 3 m and more, masses and towers block (BlockAll), so the ship, divers and the sonar meet them. Smaller stones are scenery. No shadows (the floodlights cast none).
- **Streaming.** 256 m patches: stones within one patch of the camera, everything bigger within three. The host also keeps the solid layers around every pawn and the ship. Patches are placed on worker threads (at most 8 at once) and added within 3 ms per frame as instanced meshes, one component per mesh per patch. A patch goes only when it is more than one patch beyond where it is wanted. On the first frame, the solid patches are placed at once.
- **Switch:** `-ThalassaNoClutter` turns it off, for comparisons.
- **Measured:** adding a patch costs at most about 1 ms. Helm tests with two players at the start and 12 m over the ridges: no frames over 50 ms, no corrections, the ship in step within 3 cm at the start (34 cm at worst over the ridges).

## Province looks (2026-11-09)

The ocean uses its own material, `M_OceanGround` (`create_ocean_materials.py`), and `AThalassaOcean::BuildMesh` writes each province's look into the visible tiles' vertex colours, blended across borders:

- red: volcanic, black sand and near-black basalt on slopes;
- green: pale, light fine ooze, with less gravel showing (plains fully, plateaus about half);
- blue: rocky, bare dark stone from about 15 degrees (ridges fully, canyons 70%, plateaus 60%);
- black: the plain look, as `M_SeabedGround` (hills).

The test map keeps `M_SeabedGround`. Collision tiles carry no colours.

## Not done yet

- ~~The sonar chart is one 6.4 km grid.~~ Fixed 2026-11-08: the chart covers the whole ocean, in pages, and the active ping charts the area around the ship. See `SONAR.md`.
- ~~No formations, rocks or life in the open ocean.~~ Rocks done 2026-11-09 (see Clutter above). Still no life, no big landmark formations, and no other formation families.
- **A coarse whole-ocean plan.** Today's large shapes are noise. The Thornwake way is a plan raster with vector features (trenches, ridge lines, regions and biomes), built once from the seed.
- **The terrain's character, further.** ~~Every province wears the same seabed material.~~ Since 2026-11-09 each province has its own ground (see Province looks above), but only from four texture sets, blended by slope. The province shapes also come from noise, not from a whole-ocean plan.
- **A thin straight line** showed in one frame along X = 0, a tile boundary. It may be a lighting seam between tiles. Not investigated yet.
- **Overhangs and caves** don't fit a height function.
- **Performance and memory** have not been measured with players far apart.
- **Not tested on two real PCs** (as with everything).

## Tested (2026-11-08)

- Frames: the tiles face the right way and carry the seabed material. Near the start, the abyssal plain is rolling sediment under the keel light. At the shallowest point within 20 km, a broad slope rises at about 700 m.
- Fingerprint identical across every process run.
- **Deck tour on the ocean,** with the sonar: 1 correction of 3.9 cm, no stuck waypoints. The sonar charted 10,124 cells, identical on host and client. The fingerprint was identical on host and client.
- **The old map, after all this:** 1 correction of 1.4 cm, identical sonar charts (3,736 cells; the longer range charts more).
- **Sink** (`--helm-course sink --map /Game/Maps/L_Ocean --above-ground 40`): the ship started 40 m above the highest ground under its hull, at 906 m, sank on its thrusters, and stopped on the generated seabed at 941.7 m (ground 946 m). One collision, no corrections. So the host's collision tiles stop the hull.
- **What went wrong first, for the record:** a dive test started "30 m above the ground" under the ship's centre never touched bottom, because the ground slopes away ahead. That looked like the hull passing through the seabed, but wasn't. Hence the ground-relative start, measured under the whole hull.
- **Not measured:** frame cost while tiles stream with the ship moving fast, and the first frame. The worst frame in the sink test was 261 ms, likely the first-frame collision build.
