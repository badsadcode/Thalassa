# The open ocean

The owner's direction (2026-11-08): the world must be far bigger than the 6 km test region, generated procedurally. Terrain comes first, and hand-made assets only once the terrain looks believable. The plan and the lessons from the owner's Thornwake generator are in `SEABED.md` ("A much bigger ocean"). This page is what exists.

## What exists: first version

- **`Content/Maps/L_Ocean`**, made by `Scripts/unreal/create_ocean_map.py`: the shared water look (`Scripts/unreal/thalassa_water.py`) and one `AThalassaOcean` actor. There is no baked seabed. The old test map `L_ShipMotion` is untouched, and every existing test still uses it.
- **`FThalassaSeabedGenerator`** (`Source/Thalassa/World/ThalassaSeabedGenerator.*`): the seabed as a pure function of a seed and a point, in doubles. It's made of:
  - **Basins:** abyssal plains around 2,400 m, rising to shelves around 400 m, over about 160 km.
  - **Mid-ocean ridges,** sharp-crested, over about 26 km.
  - **Canyons** cut down and across the shelves.
  - **Seamounts:** at most one per 24 km lattice cell, about half the cells, up to 2.2 km tall and 1.8 to 5.3 km across.
  - **Hills and fine detail:** hills over 3.5 km, detail over 400 m and grit over 30 m.
  - **A soft cap,** so nothing comes within 60 m of the surface.
  
  A warp keeps it all from lining up with the noise grid.
- **Determinism:** the noise uses integer hashing (SplitMix64) and polynomial smoothing only, with no `sin`, `cos`, `exp` or `pow`. Every machine builds the identical seabed from the seed. `Revision` is bumped whenever the output changes. At start, the ocean logs a fingerprint of heights on a 400 km grid. Seed 20261108, revision 1 gives **609611599508488193**, identical in every process run so far.
- **`AThalassaOcean`** (`Source/Thalassa/World/ThalassaOcean.*`) streams it, with nothing replicated:
  - **What you see:** nested rings around the local camera. There are five levels, from 256 m tiles up to 4 km tiles. The coarsest level covers ±2 tiles, about 20 km, and each finer level replaces the coarser tiles within one tile of the viewer. Tiles are 64 × 64 quads (4 m apart at level 0), and distant tiles leave out wavelengths shorter than twice their spacing. Skirts hide seams between levels. Tiles are built on worker threads, at most 12 at once, nearest level first, and added within 4 ms per frame. Old tiles stay until all their replacements exist. Each tile sits at its own corner, so its vertices stay small numbers however far out it is.
  - **What you collide with:** full-detail collision tiles, built on every machine around its own player. The host also builds them around every pawn (768 m squares) and the ship (a 1.28 km square). On the first frame they're built at once, so nothing starts over empty space.
- **The sonar** traces the world as before, then follows each beam through the height function (`AThalassaOcean::Raycast`) and keeps the nearer return. It charts generated seabed far beyond any loaded tile. Its range went from 800 m to **3 km**, like a real survey sonar: the open ocean is often over 1 km deep, and at 800 m the first tests charted nothing. The raycast takes long steps far above the ground and short ones close to it, so a 3 km beam costs a few dozen samples.
- **The ship's start:** in the open ocean, the game mode starts the ship 120 m above the seabed at the start point, no shallower than 200 m unless the ground forces it, and never within 30 m of the surface. With the default seed, the seabed is 946 m down at the start, so the ship starts at 200 m, far above anything the floodlights can reach. The log names the shallowest and deepest ground within 20 km: 704 m and 2,385 m.
- **A settling contact counts once:** a new collision counts only if the last was more than 1.5 s ago. Thrusters pushing the ship onto the seabed made it touch three times in 0.7 s.
- **Tools:** `run_motion_test.py --map /Game/Maps/L_Ocean`, `--above-ground <m>` (starts the ship that far above the highest ground under its hull) and `--helm-course sink` (thrusters full down into the seabed) and `capture_screenshots.py ... /Game/Maps/L_Ocean` play the ocean. From Git Bash, set `MSYS_NO_PATHCONV=1`, or `/Game/...` gets rewritten into a Windows path.

## Not done yet

- **The sonar chart is still one 6.4 km grid,** centred on the origin. The sonar finds ground anywhere, but only cells within 3.2 km of the origin are kept, so the chart and the holotable are empty further out. Next: chart pages keyed by tile.
- **No formations, rocks or life in the open ocean.** On purpose, until the terrain looks right. They return as per-cell placement rules (`SEABED.md`).
- **A coarse whole-ocean plan.** Today's large shapes are noise. The Thornwake way is a plan raster with vector features (trenches, ridge lines, regions and biomes), built once from the seed.
- **The terrain's character.** It's believable but gentle at the scale the floodlights see. It needs more relief and texture up close, by region.
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
