---
title: "Provinces"
date: 2026-11-08
summary: "The open ocean was deep and samey. Now it's divided into provinces, each with a landscape of its own, there's a shelf near the start, and testers get a boost button."
category: "Engineering"
tags: ["prototype", "world", "procedural", "helm"]
status: "prototype"
cover: "/images/devlog/0020-provinces.jpg"
coverAlt: "Six views of the generated seabed under the ship's floodlights: craggy plateaus, a ridge step, sharp hills, broken canyon ground, volcanic peaks and flat plains."
---

My first dive in the open ocean was impressive for one reason: it was deep. Very deep. And everywhere looked much the same: gently rolling ground, a long way down. Two things came out of that.

## Provinces

The ocean is now divided into provinces about forty kilometres across, each with a character of its own. The borders blend, so you cross from one landscape into the next rather than over a line.

- **Plains:** flat, empty sediment. The silent plains, where nothing happens for a long time.
- **Hills:** steep, rocky rises up to a couple of hundred metres.
- **Plateaus:** the ground broken into flat terraces with steep steps between them.
- **Ridges:** long, sharp ridgelines.
- **Canyons:** deep cuts with tributaries running into them.
- **Volcanic:** fields of small cones, some with craters at the top.

Close up, under the floodlights, rugged provinces now have craggy outcrops instead of smooth mud. Here's one view from each, about twenty metres above the ground:

![Six views of the generated seabed: craggy plateaus, a ridge step, sharp hills, broken canyon ground, volcanic peaks, and flat plains.](/Thalassa/images/devlog/0020-provinces.jpg)

There's also always a continental shelf around where you start now, so a new crew can reach the bottom. The abyss is still out there, waiting.

## A boost, for testing

The ship is slow, and it should be: it's heavy, and later upgrades can make it faster. But testing an ocean this size at fifteen metres per second is a slog. So in test builds there's a boost: B at the helm makes it ten times faster, up and down too. It still takes the same time to get going, so it feels like the same ship, just in a hurry. The real game won't have it.

## And a ship to look forward to

I've settled on a direction for how the ship will look: hard-surface, faceted, a bit stealthy, with a huge panoramic window at the bow and hangar bays opening onto the sea. Flat panels and decks will make the interiors much easier to fit. It's a style for now, not a blueprint. The ship still gets modelled last, once the game inside it exists.

## What this does not prove

Every province still wears the same seabed texture, so the difference is in the shapes, not the colours. Province shapes come from noise rather than from a plan of the whole ocean. I haven't driven across a border myself yet, and the sonar chart still only covers the area around the start.

## Next dive

A sonar chart that grows with the ocean, so all of this can actually be mapped. These are intentions, not promises.

*Beep. Bloop.*
