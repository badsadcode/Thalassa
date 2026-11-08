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

## Not done yet

- **More of the look:** a real 3D hologram of the chart on the bridge (the holotable is a first step), and contacts.
- **Scale.** The array of cells replicates as it grows. A few thousand cells is fine; a whole ocean would need tiles sent on request.
- **Imperfection.** Returns are exact. Real sonar has noise, shadows behind obstacles, and resolution that falls with range.
- **Hydrophones.** Only the ping exists. There is no listening yet.
- **Substrate.** Rock and sediment could return different strengths, so the chart shows what the seabed is made of. See `SEABED.md`.
