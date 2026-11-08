# Sonar: first version

The first piece of the design document's "Beep. Bloop." phase: a multibeam survey sonar that maps the seabed it actually detects.

## What it does

- **N** switches it on or off. **Tab** shows the chart beside the station view.
- While it is on, the server sweeps a fan of 48 beams, 65 degrees to either side of the ship's track and tilted 10 degrees forward, every 0.1 s. The beams are traced against real world geometry, out to 800 m.
- Each hit fills a 25 m cell of a world-fixed chart, 256 cells or 6.4 km on a side. A cell keeps its highest return, since that is what a ship would hit. Cells nobody has measured stay blank.
- The survey replicates, so everyone aboard shares one chart. Each client draws its own texture from it: deep in dark blue, shallow in pale cyan.

The code is `UThalassaSonarComponent`, on the vessel. The survey and how it is drawn are kept separate, as the design document asks.

## Tested

| Run (one each) | Median ping | Host chart | Client chart |
|---|---|---|---|
| `sonar_lan` | about 15 ms | 1,780 cells | 1,780 cells, same checksum |
| `sonar_lag` | 238 ms, 3% loss | 1,783 cells | 1,783 cells, same checksum |

Both ran the full manoeuvre profile with walkers aboard. Walking was unaffected. A frame from a stationary ship confirms the chart's orientation: the swath lies across the track, just ahead of the bow.

## Saving

The host saves the chart every 30 seconds and when the game closes, and loads it on start, so it reaches everyone who joins afterwards. It lives in `Saved/SaveGames/ShipSurvey.sav` in the project, or in the packaged game's folder. Delete that file to start a fresh chart. The file records its version, its grid and its **world**: the map's name and a seabed generation number (`ThalassaSonar::SeabedGeneration`). A chart from another grid or another world is not loaded. That's how a chart surveyed over the old pillar grid ended up drawn over the new seabed as rows of squares, on 2026-11-08. **Bump `SeabedGeneration` whenever the seabed, rocks or anything else solid out there changes shape.**

Tests never touch that file: with telemetry on, nothing is saved unless a test names its own slot. `python Scripts/build/test_survey_persistence.py` checks the whole loop. A game surveyed 36 cells and quit. A new game, with the sonar off, loaded the same 36 cells, and a client who joined afterwards received an identical chart.

## The bridge screen and the ping

The chart is also on a screen at the sonar station: the middle console on the bridge's starboard side, facing the room. It shows the same picture as the **Tab** view, because both are drawn by one function, `ThalassaSonarDisplay::Draw`. That picture is north up, 1.6 km across, with range rings every 200 m and the ship's heading in amber.

- Each process draws its own copy into a 512 px render target, from the replicated survey. Nothing new is replicated. The screen only redraws, at 30 frames a second, while it is on someone's screen.
- The material is the engine's own `Widget3DPassThrough_Opaque_OneSided`, used for 3D widgets. It is unlit, so the screen is turned down to 12% or the dark bridge's exposure makes it glow white.
- While the sonar is on, it pings every 2 seconds, on the beat of the server's clock, so everyone aboard hears the same ping. A ring spreads from the ship on the chart at the speed of sound in water, 1,500 m/s, and fades out at the edge of the view in about half a second.
- The ping is synthesised as it plays (`UThalassaSonarPingComponent`), so there is no sound asset: a 1,650 Hz tone falling slightly, a 0.3 s decay, and damped repeats 0.37 s apart. It is heard from the screen and fades out within about 30 m. It is a placeholder, like the grey deck.
- The audible ping and the survey are separate. The survey still sweeps every 0.1 s.

Tested on the deck with the sonar on: the chart matched between host and client at about 15 ms, at 149 ms with 1% loss, and at 238 ms with 3% loss. Walking matched the earlier deck runs at similar ping. Frames confirmed that the screen is the right way up and matches the **Tab** view. The ping was heard in the editor. Automated runs are silent (`-nosound`).

## The look (2026-11-08)

The chart was a debug view: one flat pixel per 25 m cell. Now it's built for the area around the ship.

- **The chart image** covers the 1.6 km around the ship plus a margin, at 6 pixels per cell. It's rebuilt locally at most ten times a second (`UThalassaSonarComponent::GetLocalChart`). Heights are smoothed between cell centres, and hill-shaded from the north-west with heights stretched six times. Colours are a navy-to-aqua depth ramp that spans whatever is in view, like a chart plotter, so a gentle plain still shows its shape. Contours are drawn every 10 m with heavier ones every 50 m over gentle ground, or 20 m and 100 m over steep ground. Freshly surveyed ground glows and fades over about a second, so you can watch the swath being painted. Unknown water is near black, with a faint 100 m grid fixed to the world. A chart that is loaded, or received on joining, does not glow.
- **The overlay:**
  - a sweep turns once per ping with a fading trail, on the server's clock, so every screen aboard sweeps together, and the ping ring still spreads at the speed of sound;
  - a compass ring with ticks every 10 degrees, and labelled range rings every 200 m;
  - an amber chevron for the ship, with its heading dashed out to the edge;
  - a readout panel: depth, water under the keel (from the survey; "--" until the ship's own cell is charted), heading and speed;
  - a depth legend for the current colour range, and faint scanlines.
- **Nothing new is replicated.** Each machine draws from the shared survey. In a deck test with the sonar on, host and client charts still match.
- **The holotable** in the mess shows the same survey in 3D: see `HOLOTABLE.md`.

Seeing it: `run_motion_test.py --deck --sonar --survey-slot SonarCanyon --ship-at=-150000,90000,-20000` charts the canyon into a save slot, and `capture_screenshots.py <folder> sonar=0@13000,0,90 -ThalassaSurveySlot=SonarCanyon -ThalassaShipAt=-90000,105000,-20000` captures the chart over it.

## The whole ocean, and the active ping (2026-11-08)

The owner found the sonar didn't do what the holotable needs. The survey paints a strip under the ship as it moves, but to plan (dive, enter the canyon, turn back) the crew need the area all around them. And far from the start, the chart was empty.

- **The chart covers the whole ocean.** Cells are addressed by whole-world cell coordinates (16 bits each way: ±819 km in 25 m cells), not by a 6.4 km grid round the origin. Each machine keeps its copy in 64 × 64-cell pages, made as the ship charts new ground.
- **It replicates as changes only** (`FSonarCellArray`, a fast array). The whole array used to be sent, which would not have survived an ocean's worth of cells. Saves are version 3; older charts are refused.
- **Two layers:** the survey's fine 25 m cells, and the ping's coarse 100 m cells. Where both know a place, the survey wins. On the chart, ping-only ground is a little dimmer and greyer, and more gently shaded, so its coarse cells don't show as squares.
- **The active ping (F, anywhere aboard):** one pulse in every direction. The wave spreads at 500 m/s (slower than sound in water, 1,500 m/s, so it can be watched) and the server charts each 100 m cell as the wave reaches it, nearest first, spread over the 2.4 s it takes. Ground hidden behind terrain stays uncharted: if the line from the ship to a point 15 m above the ground is blocked before 85% of the way there (less 30 m), it's in the ping's shadow. Pinging from higher up sees more.
- **What everyone sees and hears:** nothing new is drawn by the server. Each machine reads the replicated pulse (where, when on the server's clock, how far) and:
  - draws the ring on the seabed and rocks: `MPC_SonarPulse` (made by `Scripts/unreal/create_sonar_pulse.py`) carries the pulse to `M_SeabedGround` and `M_Rock`, which glow in a bright band at the wave front, with 5 m height contours fading out behind it (`PULSE_HLSL` in `create_ocean_materials.py`);
  - draws the ring on the chart screen, spreading from where the ping was sent;
  - plays a deep, long pulse heard all through the ship (`SonarPulse`, a `UThalassaSonarPingComponent` set up by `MakeActivePulse`).
- **Grades:** what a ship's sonar can do will come with upgrades. For testing the ship starts with the best, and **G** (test builds only) steps through them:

  | Grade | Ping | Recharge |
  |---|---|---|
  | 0, survey only | none | |
  | 1, short ping | 600 m | 12 s |
  | 2, long ping | 1,200 m | 8 s |

  The chart's readout shows the ping's range and whether it's ready.
- **The chart screen** now shows 2.4 km across, as far as the long ping reaches, and **the holotable** shows 1.2 km around the ship.
- **Test switches:** `run_motion_test.py --ping-every <seconds>` (or `-ThalassaPingEvery=`) pings from the host on a timer. `-ThalassaPulseHold=<metres>` stops the ring's picture at that radius, for screenshots; charting is unaffected.

Tested (one run each, pinging every 10 s, sonar surveying, walkers aboard):

| Run | Ping | Host chart | Client chart | Walking |
|---|---|---|---|---|
| Test map deck | 15 ms | 4,476 cells | in step: 3,746 of 3,798 client samples equal a host state; 2 cells in flight at the end | 1 correction (1.9 cm) |
| Open ocean deck | 15 ms | 4,634 cells | 4,634, same checksum | 0 corrections |
| Open ocean, 3% loss | 243 ms | 4,618 cells | in step; 1 cell in flight at the end | 21 corrections (largest 107 cm), like earlier runs at this lag |

From 30 m above the start plateau, one ping charted 292 of its 448 cells; the rest were in the ping's shadow.

## Not done yet

- **More of the look:** a real 3D hologram of the chart on the bridge (the holotable is a first step), and contacts.
- **Scale.** Changes replicate cheaply now, but a player joining late still receives the whole chart at once, and the save holds every cell. After hours of charting, both will need pages sent on request, and the chart's ±819 km limit may need lifting.
- **The ping's look, further:** a visible wave in the water itself, not only on the ground; echoes heard back from big features at the right delay; and creatures that hear it.
- **Imperfection.** Returns are exact. Real sonar has noise, shadows behind obstacles, and resolution that falls with range.
- **Hydrophones.** Only the ping exists. There is no listening yet.
- **Substrate.** Rock and sediment could return different strengths, so the chart shows what the seabed is made of. See `SEABED.md`.
