---
title: "Rocks"
date: 2026-11-09
summary: "The open ocean finally has things on the ground: stones, boulders, rock masses and the odd cluster of slab towers, placed by rules from the seed. And each kind of landscape now has its own ground colour."
category: "Engineering"
tags: ["prototype", "seabed", "procedural"]
status: "prototype"
cover: "/images/devlog/0025-ridges.jpg"
coverAlt: "Murky blue-green seabed on a ridge, with boulders and broken stones scattered across the slopes."
---

Until now the open ocean was bare ground: it had shapes, but nothing lying on them. That was on purpose, because I wanted the terrain right first. Now it has rocks.

![Boulders and broken stones scattered over a ridge.](/Thalassa/images/devlog/0025-ridges.jpg)

## Rules, not randomness

Nothing is placed by hand, and nothing is sent over the network. The ocean is divided into invisible grids, one for each kind of rock: stones every 8 metres, boulders every 32, big rock masses every 128, and towers of stacked slabs every 256. For each square, the seed decides whether something is there, where it goes, how big it is, and which rock it is. Every player's game works out the same answer, so everyone sees the same boulder in the same place, and it's still there when you come back.

The rules follow the landscape:

- **Plains** stay almost empty. Big open nothing, which I want: it makes the rare thing you do find matter.
- **Ridges** and **volcanic fields** are strewn with rock. Volcanic ground gets dark, cracked boulders.
- **Plateaus** get most of the slab towers.
- Loose rock gathers on slopes, like scree, but not on cliffs too steep to hold it.
- Some stretches are strewn and others nearly bare, so it never looks like an even sprinkle.

Boulders of 3 metres and up are solid. The ship can hit them, divers can stand on them, and the sonar sees them. Smaller stones are just scenery.

![Hills, with stones on the slopes.](/Thalassa/images/devlog/0025-hills.jpg)

## Ground with character

Each kind of landscape now has its own ground, too. Plains are pale, fine sediment. Ridges and canyon walls show bare dark stone on their slopes. Volcanic fields are black sand and basalt. Plateaus are pale on top with stony edges. Where two kinds meet, they blend over a few kilometres.

![The plains: pale, smooth and empty.](/Thalassa/images/devlog/0025-plains.jpg)

## What this does not prove

The rocks are only the scanned ones we already had: nine kinds, plus the dark boulders and slab towers. Up close, you'll start to recognise them. The colours are subtle in the murk, and I may push them further once we have better underwater light. I tested driving low over the ridges with two players, and it stayed smooth and in step, but only on my machine. There's no life on the ground yet. That's next on my wish list.

*Beep. Bloop.*
