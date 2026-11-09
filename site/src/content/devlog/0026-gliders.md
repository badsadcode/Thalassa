---
title: "Gliders"
date: 2026-11-09
summary: "Something lives down there now: schools of big, peaceful gliders. They're the same for every player without sending a byte, and the ping scares them."
category: "Engineering"
tags: ["prototype", "creatures", "sonar"]
status: "prototype"
cover: "/images/devlog/0026-gliders.jpg"
coverAlt: "A school of dark, broad-winged gliders over a murky seabed, each with a faint glowing spot near its wingtips."
---

The ocean had ground, rocks, sonar and sound, but nothing alive in it. Now it has its first creature.

![A school of gliders over the seabed, their wingtips glowing faintly.](/Thalassa/images/devlog/0026-gliders.jpg)

## The glider

A big, gentle animal, about five metres across the wings, with a dark back, pale lines along the wings, a long whip of a tail, and a faint glowing spot near each wingtip. They glide in loose schools of five to eleven, a few dozen metres above the ground, following its shape, slowly beating their wings and banking into turns.

They're placeholders, like the grey deck: made by rules, so I can get the behaviour right before anyone designs what they really look like.

## The same animals for everyone

Same trick as the seabed and the rocks: nothing about them is sent over the network. The seed decides where each school lives and who's in it. Where a school is at any moment comes from the game's clock, which every player shares. So everyone sees the same animals in the same places, doing the same thing, and it costs no bandwidth. There's always a school near the start, so you can meet them straight away.

## The ping scares them

When the ping's wave reaches a school, the animals bolt away from the ship, spread out, beat their wings much faster, and their glowing spots flash. Over the next half minute they calm down, gather and drift back. So the ping isn't free any more: it maps the ground, but it tells everything around you that you're there.

![The same school after a ping: scattered and fleeing.](/Thalassa/images/devlog/0026-scatter.jpg)

And the ping sees them. Everything alive its wave touches shows on the sonar chart as an amber mark, which fades over a few seconds. It's where the animal was when the wave hit it, not where it is now. A school shows up as a little cluster.

![The sonar chart with amber contacts around the ship.](/Thalassa/images/devlog/0026-contacts.jpg)

## A bug I found on the way

While testing, I spotted a warning in the game's log: the scanned rocks I added last time were missing a setting. In the editor it fixes itself, so they looked fine. In a real build of the game, every rock in the open ocean would have been drawn with Unreal's grey default material. Fixed before anyone saw it.

## What this does not prove

I tested it with two players on the open ocean, pinging every ten seconds. Walking and the ship were as smooth as before, and both players' sonar found exactly the same animals, ping after ping. One exception: a player who joins late doesn't know about pings from before they arrived, so for up to a minute their gliders can be calmer than everyone else's. All of it ran on one machine, and the animals are silent: there's no hydrophone yet, so you can't hear them.

Next, the fun part: lots more life. Small fish, big fish, giant jellyfish, some of them glowing, and one very large thing that's bigger than the ship.

*Beep. Bloop.*
