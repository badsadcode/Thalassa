# The holotable

The owner's idea (2026-11-08): a table aboard with a 3D hologram of the terrain below and around the ship, a little simplified, moving as the ship moves. A place where the crew gather and decide what to do next.

## What exists: first version

- **Where:** the sunken centre of the mess hall, between the mess tables, at ship coordinates (-1950, -450), top at 40 cm. It's the `HoloTable` component in `BP_Thalassa` (Thalassa | Spots): move it and the table and hologram move with it.
- **What it shows:** the charted seabed within 800 m of the ship, ahead, astern and abeam, as a point cloud of 96 × 96 points on a 2 m table. Only what the sonar has surveyed appears, so the hologram fills in as the ship maps the seabed.
- **Heading-up:** the bow always points the same way across the room, so ahead on the table is ahead in the ship.
- **Heights** are stretched three times, so gentle ground still has shape, but the relief never stands taller than 40 cm. Steep ground is flattened to fit.
- **Colours** use the chart's depth palette, spanning whatever is in view.
- **The ship** is an amber chevron at its own height above the terrain, about its true size at the table's scale (280 m is about 35 cm), with a dotted line dropped to the seabed beneath it.
- **The material** `Content/Ship/Holo/M_HoloPoint` (made by `Scripts/unreal/create_holo_material.py`) is unlit and additive, with a slow shimmer rising through it.
- **Network:** each machine builds its own hologram from the replicated sonar survey, five times a second. Nothing new is replicated, so host and clients show the same picture because they share the survey.
- **Cost:** about 9,000 instanced points, updated five times a second, on machines that draw. Not built on a dedicated server.

## Not done yet

- **The table is scenery,** so players walk through it. Giving it collision means a box in the walkable shell (`SHIP_ART.md`).
- **No interaction.** Ideas: point at the table to drop a shared marker ("there"), so a crew member can show the helmsman; zoom between 400 m and 3 km; tilt; switch heading-up to north-up.
- **Sonar contacts** on it, when contacts exist: other vessels, creatures, rare sounds by bearing.
- **The look:** a projector glow on the table top, scanlines, flicker when the ship is hit, a soft edge where the hologram ends. The points still lean white under bloom; colours could be stronger.
- **Detail** is limited by the sonar's 25 m cells. Finer sonar resolution near the ship (`SEABED.md`) would sharpen it.

## Tested

Frames over the canyon, from a survey made by the scripted manoeuvre run (`run_motion_test.py --survey-slot SonarCanyon --ship-at=-150000,90000,-20000`), then captured with `-ThalassaSurveySlot=SonarCanyon -ThalassaShipAt=-90000,105000,-20000`. The canyon reads clearly, with the ship marker and the dropline. It hasn't been seen with the ship moving, except in reasoning: it rebuilds every 0.2 s from the ship's pose.
