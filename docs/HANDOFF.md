# Handoff: THALASSA, updated 2026-11-07

Read this first in a new session. Then read `README.md`, `docs/Decisions.md`, `docs/MOVING_SHIP_SPIKE.md`, `docs/INTERIOR_RULES.md`, `docs/SONAR.md`, `docs/HELM.md` and `docs/STATIONS.md`.

## The project

A first-person 1 to 8 player co-op submarine game in Unreal Engine 5.8.4, C++. Studio: Bad Ideas Works. Owner: Tom (GitHub `badsadcode`). Public repo: `badsadcode/Thalassa`. Devlog: https://badsadcode.github.io/Thalassa/ (Astro in `site/`, deployed by GitHub Actions on push to `site/**`).

The owner's design documents are in `docs/` but **deliberately untracked**: `THALASSA_Master_Design_Document.md`, `THALASSA_Devlog_Website_Brief.md`, `Thalassa_Sage_Authorization.md`, and three concept images. Never `git add docs/` or `git add -A`. Stage files by name.

## What exists and works

All of it is tested headless with a real listen host and clients as separate processes.

- **Moving ship (approach A):** a kinematic ship, with pose snapshots replicated by the vessel and characters standing on it by based movement. Corrections are near zero with 2 to 4 players at normal ping. See `MOVING_SHIP_SPIKE.md`.
- **Fixes that must stay:** all hull collision in one component (`HullCollision`); `p.NetServerMaxMoveDeltaTimeScalar=1` in `DefaultEngine.ini`; the client ship clock taken from snapshots; rotation rates sent as an `FVector`, not an `FRotator`.
- **Soft crew collision:** capsules ignore each other, and the controlling client adds a small movement input away from anyone within 90 cm.
- **Airlock and swimming:** water is decided by ship compartment, and swimming is `MOVE_Flying`. The base is kept in the flooded chamber and dropped past the outer door. Doors stop being solid as soon as they start opening. The scripted diver completes repeated EVAs at up to 238 ms ping.
- **Grey main deck,** the editor default: about 280 m, with a ring corridor, sunken mess, tiered bridge, engineering hall with stairs (hidden ramp) and a ladder (`MOVE_Flying` climbing zone), and the airlock on the hull. It is deliberately unpolished, because the owner said the art will replace it. `-ThalassaTestRig` gives the old three-room rig, and the motion tests use it by default.
- **Sonar:** **N** toggles it, and **Tab** shows the chart. 48 beams, 25 m world-fixed cells, replicated, saved to `Saved/SaveGames/ShipSurvey.sav`. Host and client charts are identical, even with packet loss.
- **Ladders on a moving ship (fixed):** anyone reaching a ladder while the ship moved was thrown astern, because the switch to climbing counted the ship's velocity twice. It was recorded as a two-player problem, but the ship's motion caused it. After the fix, four players all climbed, sometimes two at once. Deck runs at about 135 ms went from 122 corrections to 6, and no waypoints are stuck. See `INTERIOR_RULES.md` rule 10.
- **Helm (first version):** E at the console by the bridge's forward window takes the helm. WASD, Space and C drive the ship, R switches between precise and set-and-go, and X is all stop. The throttle lever, the helm screen and the HUD all show the same replicated levers. The ship is heavy: about 30 seconds to near full speed, and a long coast. The old test keys (I/K, J/L, U/O) are gone. Tested with a scripted helmsman at about 26 ms and 240 ms. See `HELM.md`.
- **Full stop:** X at the helm, or the red button on the helm console that anyone can press. The ship brakes to a standstill and holds until the helmsman touches a control. See `HELM.md`.
- **Sprinting:** Shift, at 8 m/s on foot. It is part of each saved move, so the server agrees with the client. Grabbing a ladder slows you to climbing speed, or a sprinter crossed the climbing zone and bounced off. `--sprint` makes test walkers sprint.
- **Stations and the camera station:** helm and cameras share one station class (E to take, E to leave, leave if pushed away). Six hull cameras show on a large and a grid screen at the starboard console beside the sonar. Cameras render only while someone can see them. See `STATIONS.md`.
- **Surface and seabed:** under the helm, the ship floats at 15 m at most and stops dead against the seabed or anything solid, including a pitch or turn that would swing the hull into rock. The helm shows SURFACED and flashes COLLISION. The scripted motion test is not constrained. See `HELM.md`.
- **Sonar station on the bridge:** a screen above the starboard middle console shows the same chart as **Tab**, with a ping every 2 s on the server's clock. The ping is synthesised in code, with no sound asset. The owner heard it in the editor on 2026-11-07. Automated runs are silent (`-nosound`).
- **Packaging:** `Scripts/build/package-windows.ps1` writes to `D:\ThalassaBuilds\Windows`, with `Host.bat`, `Join.bat` and a README. Repackaged on 2026-11-07 with the sonar screen and the ladder fix. Tested as host plus client on one PC: a deck run with the sonar on had identical charts, no corrections and no stuck waypoints, and a frame showed the bridge screen working.

## Open problems

1. **High-ping corrections at the airlock's outer door,** where a character switches between ship frame and world frame. A theory, not confirmed: the client shows the ship slightly late. The ladder fix changed the same switch where the water starts: on LAN the largest dive correction fell from 17.5 to 8.7 cm, but at 238 ms the results barely moved (209 corrections, then 194).
2. **The "visible snap" metric sometimes read 16 to 40 cm when pops read 0.** Recheck it before chasing it: the deck numbers it came from were dominated by the ladder bug, now fixed.
3. **Never tested on two real PCs.** The owner has no second PC soon, and is behind CGNAT, so cannot host directly.

## Owner's direction, not yet built

All of this is recorded in `Decisions.md`.

- **Internet play:** playit.gg UDP tunnel to port 7777 for testing (untested), Epic Online Services peer-to-peer for the real game. Friends must never install anything. EOS needs the owner's Epic developer account first.
- **Helm, the rest of it:** steering that depends on ship systems, a real seat, a visible rudder and planes, and tuning by someone actually playing. The first version is built: see `HELM.md`.
- **Layout:** not a cigar. Looping corridors, asymmetry, so you get lost at first, stairs, ladders, and floors at different levels.
- **Sonar look:** high-tech and polished: a visible sweep, contours, a hologram, fine point clouds. The current chart is a debug view. The data layer needs finer local resolution, maybe 1 to 2 m.
- **Mysteries:** the Long Call and the Viator are never objectives. Rare sounds need a stable signature identity in the hydrophone archive.

## Suggested next steps

The owner's direction on movement: smooth and fluid like Titanfall (see `Decisions.md`), so a slide is a natural next move, with the same network testing as walking. Also: something to do when the ship hits the seabed (a bump everyone feels the same, a sound), hull floodlights for the cameras, more monitors, or internet play with playit.gg.

## How to work here

- **Build:** `powershell -NoProfile -File Scripts/build/build-editor.ps1`. **The Unreal editor must be closed**, or the build fails. Ask the owner to close it.
- **Tests:** `python Scripts/build/run_motion_test.py --name X [--deck] [--eva] [--sonar] [--rtt 100 --jitter 10 --loss 1] [--clients 3] [--helm] [--exe <packaged exe>]`. The full set is `run_motion_matrix.py`. Results go to `%TEMP%\ThalassaMotionTests`, never the slow `H:` drive, because writing there stalls the game. Read **visible pops**, not raw correction size, which grows with ping.
- **See it:** `python Scripts/build/capture_screenshots.py <folder> name=yaw@x,y,z`, in ship coordinates. A name starting `sonar` opens the **Tab** view with the sonar on. A name starting `screen` switches the sonar on and leaves the view clear: `screen=90@11200,450,120` looks at the bridge screen. Always look at frames: headless tests missed a black interior and a hole in the hull.
- **Map:** `Scripts/unreal/create_test_map.py` rebuilds `Content/TestMaps/L_ShipMotion` in place.
- **Opening the project:** the owner uses the THALASSA desktop shortcut, which points straight at the 5.8 editor. Double-clicking the `.uproject` fails, because the registry has no 5.8 entry.
- **Shell:** Git Bash rewrites `/Game/...` arguments, so launch Unreal from Python. Very long single commands fail, so put scripts in files. Files are LF in the working tree.
- **Owner preferences:** asks before every push, then says "Yes, push it". Wants honest devlog posts, written in his voice, never overstating. Don't polish placeholder art. His usage limit matters, so stop chasing low-value bugs and record them instead.
- **Commit trailer:** `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`
