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

The host saves the chart every 30 seconds and when the game closes, and loads it on start, so it reaches everyone who joins afterwards. It lives in `Saved/SaveGames/ShipSurvey.sav` in the project, or in the packaged game's folder. Delete that file to start a fresh chart. The file records its version and grid, and a chart from a different grid is not loaded.

Tests never touch that file: with telemetry on, nothing is saved unless a test names its own slot. `python Scripts/build/test_survey_persistence.py` checks the whole loop. A game surveyed 36 cells and quit. A new game, with the sonar off, loaded the same 36 cells, and a client who joined afterwards received an identical chart.

## The bridge screen and the ping

The chart is also on a screen at the sonar station: the middle console on the bridge's starboard side, facing the room. It shows the same picture as the **Tab** view, because both are drawn by one function, `ThalassaSonarDisplay::Draw`. That picture is north up, 1.6 km across, with range rings every 200 m and the ship's heading in amber.

- Each process draws its own copy into a 512 px render target, from the replicated survey. Nothing new is replicated. The screen only redraws, at 30 frames a second, while it is on someone's screen.
- The material is the engine's own `Widget3DPassThrough_Opaque_OneSided`, used for 3D widgets. It is unlit, so the screen is turned down to 12% or the dark bridge's exposure makes it glow white.
- While the sonar is on, it pings every 2 seconds, on the beat of the server's clock, so everyone aboard hears the same ping. A ring spreads from the ship on the chart at the speed of sound in water, 1,500 m/s, and fades out at the edge of the view in about half a second.
- The ping is synthesised as it plays (`UThalassaSonarPingComponent`), so there is no sound asset: a 1,650 Hz tone falling slightly, a 0.3 s decay, and damped repeats 0.37 s apart. It is heard from the screen and fades out within about 30 m. It is a placeholder, like the grey deck.
- The audible ping and the survey are separate. The survey still sweeps every 0.1 s.

Tested on the deck with the sonar on: the chart matched between host and client at about 15 ms, at 149 ms with 1% loss, and at 238 ms with 3% loss. Walking matched the earlier deck runs at similar ping. Frames confirmed that the screen is the right way up and matches the **Tab** view. The ping was heard in the editor. Automated runs are silent (`-nosound`).

## Not done yet

- **The look.** The screen is the debug chart on a flat panel. The design direction is a polished, high-tech display.
- **Scale.** The array of cells replicates as it grows. A few thousand cells is fine; a whole ocean would need tiles sent on request.
- **Imperfection.** Returns are exact. Real sonar has noise, shadows behind obstacles, and resolution that falls with range.
- **Hydrophones.** Only the ping exists. There is no listening yet.
- **Something worth mapping.** The test map is a flat seabed with square pillars. Real terrain will make the chart interesting.
