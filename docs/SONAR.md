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

## Not done yet

- **A physical console screen.** The chart is only on the HUD. The design document wants the same data on a console in the world.
- **Scale.** The array of cells replicates as it grows. A few thousand cells is fine; a whole ocean would need tiles sent on request.
- **Imperfection.** Returns are exact. Real sonar has noise, shadows behind obstacles, and resolution that falls with range.
- **Sound.** No ping, no hydrophones yet.
- **Something worth mapping.** The test map is a flat seabed with square pillars. Real terrain will make the chart interesting.
