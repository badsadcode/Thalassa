---
title: "Real Surfaces"
date: 2026-11-08
summary: "The rocks and the seabed now wear scanned textures of real rock, sediment and gravel, all free to use."
category: "Engineering"
tags: ["prototype", "world", "art"]
status: "prototype"
cover: "/images/devlog/0015-after.jpg"
coverAlt: "A layered rock tower on a seabed slope, its surface rough and fractured, the ground around it a mix of sediment and gravel, in murky blue-green water."
---

The rocks from the last post had the right shapes but flat colour, and up close they looked like clay. Shape is only half of what makes rock look like rock. The other half is surface: grain, cracks, pits, the way light catches tiny edges.

## Scanned, and free

The usual source for that in Unreal is Epic's Megascans library. It needs an Epic account and a licence check for each asset. For a first try, I used two libraries that need neither: Poly Haven and ambientCG. Their textures are photographed from real surfaces and released as CC0, which means anyone can use them for anything.

Four went in: a dark, layered, fractured rock for the formations, rippled sand for the flat seabed, coral gravel for the slopes, and a damp sand to try later. The rock still carries the layers and mineral streaks painted into each formation, so the texture adds detail without hiding the geology.

Before and after, the same view:

![Before: a stacked rock tower with flat, smooth colour on a plain seabed slope.](/Thalassa/images/devlog/0015-before.jpg)

![After: the same tower with a rough, fractured rock surface, and the seabed around it showing sediment and patches of gravel.](/Thalassa/images/devlog/0015-after.jpg)

## Hiding the repeat

The first try had a classic problem: the ground showed a regular pattern repeating every six metres, the giveaway of a tiled texture. Now each texture is mixed with a copy of itself at about three times the size, in a pattern that wanders across the seabed, so the repeat never lines up twice. The flats are sediment, and where the ground steepens it turns to gravel, along an edge that wanders too.

Under the keel floodlight, the seabed now looks like this:

![The seabed from just under the ship: a rippled sediment floor lit by a cone of light from above, fading into the dark.](/Thalassa/images/devlog/0015-sediment.jpg)

## What this does not prove

These are still frames, taken close to things. I haven't steered around it yet. Every rock wears the same texture. Regions with their own character, sediment settling on ledges, and crusts of minerals are all still to come. Megascans may follow for more variety.

## Next dive

Steering out to the monolith myself, at last. These are intentions, not promises.

*Beep. Bloop.*
