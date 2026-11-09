---
title: "Water Inside"
date: 2026-11-09
summary: "The water in the airlock and in flooded rooms finally looks like water: waves, refraction, colour by depth, foam at the walls, and a proper view from underneath."
category: "Engineering"
tags: ["prototype", "rendering", "water", "damage"]
status: "prototype"
cover: "/images/devlog/0032-shallow.jpg"
coverAlt: "A flooded corridor: shallow teal water over the floor, lamp glints on the ripples, foam lines along the walls."
---

Until now, water inside the ship was a flat, blue-green sheet. It rose in the airlock and in flooded rooms, and that was about it. Now that the ship can take on water, it had to look like water.

![Shallow water in the aft corridor. You can see the floor through it, lamp glints on the ripples, and foam where it meets the walls.](/Thalassa/images/devlog/0032-shallow.jpg)

## The surface moves

The water is now a fine mesh rather than two triangles, so the shader can actually move it:

- **Real waves.** A handful of wavelengths, each moving at the speed real water waves of that length move. Calm water barely stirs. Water pouring in through a breach, or the sea filling the airlock, churns it up.
- **Slosh.** The game watches how the ship moves, and the water tilts the way it would in a tank: it piles up at the back when you speed up and rocks back and forth after a jolt. Long, shallow rooms slosh slowly and short, deep ones quickly, just like real tanks.
- **Ripples** too small for the mesh go into the shading instead.

## The surface shades

- **You see through it, bent.** What's under the water shows through, shifted by the ripples, so the floor wobbles.
- **Colour by depth.** The deeper the water, the more colour it takes away from what's underneath: red goes first, then green. A few centimetres over the floor looks clear. A metre down it's deep teal.
- **Reflections.** At a shallow angle it turns into a mirror, and the ship's lamps glint on it, red when the alarm is on.
- **Foam** gathers where the water meets the walls, and in white patches when it's churned up.

![Half a metre of water in the same corridor: deep teal, the floor almost gone.](/Thalassa/images/devlog/0032-deep.jpg)

## And from underneath

Swim under it and the view changes too. Colours fade with distance, red first. Things blur into murk lit by whatever light is around. The picture wavers a little. Look up and the surface lets light through only in a circle above you, the way real water does (Snell's window). Outside that circle it's a mirror.

![Looking up from under the water: a lamp's light coming through the surface.](/Thalassa/images/devlog/0032-below.jpg)

## What this does not prove

These are still pictures from test runs with the water held at a fixed level. I haven't yet watched it slosh while the ship is actually flying and crashing, or seen the churn from a real breach. The biggest room, midships, is dark right now, so I tested in the aft corridor instead. Caustics, the rippling light patterns on the floor under water, aren't in yet.

*Blub.*
