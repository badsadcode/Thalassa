---
title: "Landmarks and Eyes"
date: 2026-11-09
summary: "My first AI-made rock formations, testing whether that's a good way to make assets. And proper eyes for the leviathan and the krakens."
category: "Engineering"
tags: ["prototype", "art", "seabed", "creatures"]
status: "prototype"
cover: "/images/devlog/0029-arch.jpg"
coverAlt: "A huge weathered stone arch on the murky seabed, lit by the ship's floodlights."
---

I wanted to know whether I could make rock formations quickly myself, with Meshy AI. So I made four: a layered ledge, an arch, a cluster of stones and a tall monolith.

![A 55-metre stone arch standing on the seabed, lit by the floodlights.](/Thalassa/images/devlog/0029-arch.jpg)

## Does it work?

Yes. They're in the open ocean now as rare landmarks, scattered by the same seed rules as the rocks, mostly in canyons and on ridges. There's one of each near the start: the arch stands right in front of where the ship begins, like a gateway. They're solid, so the ship can bump into them.

![The monolith, a tall pillar of weathered stone.](/Thalassa/images/devlog/0029-monolith.jpg)

A few things I learned:

- **A million triangles adds nothing.** My first two exports were over a million triangles each, with 4K textures. Cut down to 30,000 triangles and 2K textures, they look the same in the game at a quarter of the size. That's the budget from now on, and a script cuts down anything bigger automatically.
- **In the deep, texture barely matters.** I wasn't happy with the textures Meshy made, so I tried colouring the rocks with our own scanned rock instead. Side by side under the floodlights, I could hardly tell them apart. In murky water what you see is the shape and the relief. I kept our scanned rock, so the formations match the rest of the seabed.
- **Mind the pivot.** Meshy puts a model's centre in the middle of it, not at its base, so at first my arch was half buried.

![The layered ledge, its stone plates stacked like a shelf.](/Thalassa/images/devlog/0029-ledge.jpg)

## Eyes

I also made eye textures for the monsters. The leviathan now has a green, upright slit pupil, glowing faintly from inside. The krakens get an orange eye with a horizontal slit, like real squid and octopuses.

![The leviathan's green slit eye and the kraken's orange one, on their placeholder bodies.](/Thalassa/images/devlog/0029-eyes.jpg)

## What's next

That's the start of what I'm calling phase one: **a leviathan hunt**. Ship systems that can be damaged and repaired, flooding, harpoons and torpedoes, and a leviathan that stops being merely curious and starts hunting back.

*Beep. Bloop.*
