---
title: "Softer Lights"
date: 2026-11-08
summary: "The floodlights were too strong and too even. Now each beam has a hot centre, a soft edge and some texture, and the next sonar tool is decided."
category: "Engineering"
tags: ["prototype", "lighting", "sonar", "atmosphere"]
status: "prototype"
cover: "/images/devlog/0022-floodlights.jpg"
coverAlt: "Before and after: a keel floodlight as a blown-out white oval on the seabed, then as a softer, textured pool of light, and a beam visible as a shaft in the water."
---

I spent some time down there, and something about the floodlights bothered me. They were too strong and too even: every lamp made the same crisp white oval on the seabed, like a stage spotlight. If you make them too weak, they don't light anything, so I wasn't sure what the fix was.

## Lamps, not spotlights

It turns out several things were wrong at once:

- **They were far too bright.** On the ground thirty metres below, the centre of the beam clipped to pure white. That hid everything else. They're now about a third as bright.
- **The light was even all the way across.** Now each beam has a hot centre that fades out to a wide, soft edge.
- **No texture.** Each beam now has a faint mottled pattern, like a lamp behind a worn, scratched housing. Every lamp gets its own version, so no two pools of light look alike.
- **All identical.** Each lamp now has its own brightness, slightly different colour and aim, so where they overlap the light is uneven, like real lamps of different ages.

Being dimmer, the beams stopped showing in the water, so I turned up how much they light the murk. You can see the shafts again.

![Top left: before, a blown-out white oval. Top right: after, a softer textured pool. Bottom: closer to the ground, and the beam as a shaft in the water.](/Thalassa/images/devlog/0022-floodlights.jpg)

## A ping, coming

The other thing that felt off was the sonar. It maps a strip under the ship as you move, which is how real survey ships work. But the holotable is meant for planning: you gather round it and decide whether to dive, go into the canyon or turn back. For that you need the area around you, not a trail behind you.

So the next tool is an **active ping**: one loud pulse in every direction. A ring of light sweeps across the terrain as it goes out, and the holotable fills in from the centre. Ships will get it, and its range, through upgrades. A loud ping might one day draw attention from things you'd rather not meet, so the choice is yours: ping, or creep in blind.

## What this does not prove

I've only looked at the lights in screenshots, at a few depths. They may be too dim right up against a cliff, or too faint at a hundred metres. That's tuning, and easy to adjust. The ping isn't built yet.

*Beep. Bloop.*
