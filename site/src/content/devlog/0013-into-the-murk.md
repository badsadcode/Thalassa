---
title: "Into the Murk"
date: 2026-11-08
summary: "The ship stopped flying over a city of grey pillars. Now there's a real seabed under it, murky water around it, floodlights cutting through the dark, a sonar chart worth looking at, and a hologram table in the mess."
category: "Engineering"
tags: ["prototype", "world", "sonar", "lighting", "atmosphere"]
status: "prototype"
cover: "/images/devlog/0013-after.jpg"
coverAlt: "In-engine view from the water: the grey-box ship hangs in dark, murky teal water, with two white floodlight beams from its bow and one from its keel cutting down toward a faint seabed."
---

Until this morning, the sea in Thalassa looked like this:

![The ship hanging in bright, clear cyan water above a flat floor covered in a grid of tall grey box pillars, like a city seen from an airship.](/Thalassa/images/devlog/0013-before.jpg)

That's a Zeppelin over a city, not a submarine. The pillars were useful for testing, because they make the ship's motion easy to read, but they had to go.

## A seabed

There's now a real seafloor: 6 by 6 kilometres, generated in Blender. Under the ship's starting point is a basin 500 metres down. Further out, ridges rise, a canyon winds along one side, and a seamount climbs to within a hundred metres of the surface. It's generated from a fixed recipe, so it's exactly the same every time, and the sonar always charts the same thing.

It's still just rolling ground, though. I've written down what I actually want the seabed to become: slab towers, monoliths, arches, volcanic fields, deliberately empty plains, and the odd formation big enough to make the ship feel small. That's a long road, and this is the first step on it.

## Murky water

Then the water. It's dark and murky now, lit only by a dim, cold light from far above. The camera no longer brightens itself to compensate, so dark stays dark. The ship has five floodlights on its hull, two at the bow and three under the keel, and because the water itself catches light, they cast real beams. Tiny specks of marine snow drift everywhere and show up when a beam passes through them. When the ship moves, they stream past the windows.

![The same view after: murky dark-teal water, the ship a dark silhouette, white floodlight beams fanning down from its bow and keel toward a faint seabed.](/Thalassa/images/devlog/0013-after.jpg)

From the bridge window you now see mostly darkness, the glow of the bow lamps, and the snow. At 200 metres down, that's about right.

## A sonar chart worth looking at

The sonar used to draw one flat square per 25 metres. Now its chart is shaded like a relief map, with smooth contour lines and colours that adapt to what's on screen. A sweep turns in time with the ping, there's a compass ring, and a readout shows the depth and how much water is under the keel. Freshly mapped ground glows briefly, so you can watch the sonar paint the seabed as you go. Here it is after a run along the canyon:

![The sonar display: a hill-shaded chart of a canyon in dark blue with pale ridges either side, contour lines, range rings, a compass ring, an amber ship chevron in the middle, and readouts for depth, keel clearance, heading and speed.](/Thalassa/images/devlog/0013-sonar.jpg)

## The holotable

And one more thing, which I came up with in the middle of all this: a table in the mess hall with a hologram of the seabed around the ship. It's a glowing point cloud of everything the sonar has charted within 800 metres, with the ship as an amber marker and a line dropped to the bottom beneath it. Ahead on the table is always ahead in the ship. It's meant to be the place where the crew gather to decide where to go next.

![The holotable in a dark mess hall: a shimmering cyan point cloud of canyon terrain floating over a table, with small amber points marking the ship.](/Thalassa/images/devlog/0013-holotable.jpg)

## What this does not prove

None of this is final art. The seabed is smooth rolling ground with no rocks yet, and its material is a placeholder. The hologram's colours wash out a bit, the table has no collision yet, and you can't interact with it. I've seen all of this in still frames, not yet steering through it myself. The tests say walking, steering, diving and the shared sonar chart still work as before. But how it feels to dive into a dark canyon with the floodlights on is something I'll only know once I've done it.

## Next dive

Steer into that canyon myself. Then the first real rock formations on the seabed. These are intentions, not promises.

*Beep. Bloop.*
