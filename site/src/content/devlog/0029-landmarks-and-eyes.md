---
title: "Landmarks and Eyes"
date: 2026-11-09
summary: "My first AI-made rock formations, proper eyes for the monsters, a kraken I made myself, and why everything down there looks so dark."
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

## Kraken 2.0

Then I made a kraken myself: a body, eight limbs and two eyes, textured, with a properly mean head. It's in the game now as the elder kraken, the old krakens' long-lost cousin, 140 to 220 metres long. The old krakens flee from a ping. This one turns and comes closer.

![The elder kraken's limbs, lined with suckers, hanging over the seabed.](/Thalassa/images/devlog/0029-elder.jpg)

## Why is everything so dark?

Testing all this, I kept wondering why so much of it turns black. Here's the answer. Down there, the only light is a faint glow from above and some ambient light that also comes only from above. Anything facing sideways or down gets almost nothing, so creatures' flanks, the sides of the arch and the kraken's arms show up as black shapes against the bright water.

Turning that ambient light up didn't help, because it was made from a picture of the surroundings, and underwater that's dark water. So I tried giving it an even source instead: the water's own colour, coming from every direction, the way light scatters in real water. On the left, how it is now. On the right, the test.

![The same view twice: on the left, the creatures as dark shapes; on the right, with even light from the water, their forms and colours showing.](/Thalassa/images/devlog/0029-light.jpg)

I like the right one better, but it's a big change to the mood, so it's a test switch for now.

## What's next

That's the start of what I'm calling phase one: **a leviathan hunt**. Ship systems that can be damaged and repaired, flooding, harpoons and torpedoes, and a leviathan that stops being merely curious and starts hunting back.

*Beep. Bloop.*
