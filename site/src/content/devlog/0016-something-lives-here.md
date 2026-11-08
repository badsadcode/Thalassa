---
title: "Something Lives Here"
date: 2026-11-08
summary: "The first biome: a dark corner of the seabed with fractured boulders, tube worms around warm seeps, sea pens, long swaying whips, and small lights that belong to living things."
category: "Engineering"
tags: ["prototype", "world", "life", "lighting"]
status: "prototype"
cover: "/images/devlog/0016-seep.jpg"
coverAlt: "A warm seep on the seabed in a cone of floodlight: clusters of pale tube worms with red plumes, small glowing blue-green bulbs scattered in the darkness around them."
---

When I saw the textured rocks for the first time, my thalassophobia (undiagnosed) started tingling. That's exactly the feeling I want this game to give. The seabed needs more than rocks for that, though: it needs things that live there. Different parts of the ocean should feel different. Some dark and gloomy, some a little brighter with glowing plants and animals.

## The first biome

The first one is dark. It's in a corner of the map, about two and a half kilometres from where the ship starts:

- **Rocks:** clusters of dark, fractured boulders.
- **Seeps:** four warm seeps, crowded with tube worms, pale tubes with red plumes.
- **Sea pens:** patches of them on the soft ground, like feathers on a stalk.
- **Sea whips:** long, thin stalks, up to almost five metres, on the slopes.
- **Glow bulbs:** small clusters of stalked bulbs, the brightest life there.

Everything was generated in Blender, the same way as the rocks, so every plant and animal comes in a few variants.

![A warm seep in the ship's keel floodlight: tube worms with red plumes, glowing blue-green bulbs scattered around in the dark.](/Thalassa/images/devlog/0016-seep.jpg)

## Light that lives

Down here, the only light besides the ship's own comes from life. The glows aren't real lights: there are hundreds of them, and real lights would sink the frame rate. They're materials that shine, each pulsing slowly at its own pace. The plants and animals also sway in the current, each on its own beat, so the field never moves in lockstep. The whole corner is a little colder and darker than the open seabed around it.

From a little further away, it's mostly darkness: the keel light making a cone in black water, and here and there a faint glow that wasn't put there by people.

![The keel floodlight as a cone of light in dark water, reaching down to the seep, with a few faint glows scattered in the darkness.](/Thalassa/images/devlog/0016-dark.jpg)

## What this does not prove

The swaying and the pulsing don't show in still frames, and I haven't swum through it yet. Nothing moves on its own: there are no creatures, and the current doesn't really flow anywhere yet, everything just sways in place. One dark corner isn't a world of biomes. The brighter, fluorescent one is still to come. And a 795-piece field of life is small. The real ocean will need far more, and I haven't measured what that costs yet.

## Next dive

A brighter biome with fluorescent life, and the first creature. Probably heard before it's seen. These are intentions, not promises.

*Beep. Bloop.*
