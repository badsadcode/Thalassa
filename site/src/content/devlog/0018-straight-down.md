---
title: "Straight Down"
date: 2026-11-08
summary: "I steered through the new ocean myself. Out of it came vertical thrusters, the fix for a ghost on the sonar, and the realisation that the world is far too small."
category: "Engineering"
tags: ["prototype", "helm", "sonar", "world"]
status: "prototype"
---

I finally took the helm in the new ocean, with the sounds on. The atmosphere is there. The machinery is a bit synthetic, but it does the job, and the dark does the rest. Three things came out of it.

## Thrusters

Diving meant tipping the whole ship nose-down with the planes and driving forward. Sometimes you just want to go straight down, or creep up to a ledge without pointing the bow at it. So the ship now has vertical thrusters: Space and C lift or sink it, perfectly level. They're heavy, like everything else on this ship: about twelve seconds to reach full vertical speed, a gentle metre and a half per second, and it keeps drifting for a while after you let go. The planes moved to Q and Z, so you still have both.

In the test, the ship sank 37 metres and rose 45 without tipping by even a tenth of a degree, with the walkers aboard undisturbed.

## A ghost on the sonar

My sonar showed rows of neat squares over the new seabed. They were the pillars of the old test world, surveyed weeks ago and saved, now drawn on top of terrain where they no longer exist. A nice ghost story for the game, actually, but not as a bug. Saved charts now remember which world they were made in, and a chart from a different seabed is no longer loaded.

## The world is too small

The test ocean is six kilometres across. At cruising speed you cross it in seven minutes, and I could feel the edges. The real ocean needs to be vastly bigger. My other project, a survival game in Godot, already generates worlds of 4,096 by 4,096 kilometres. The way it does that should carry over: build everything from a seed, generate the ground in tiles around whoever is there, and place every rock by rules that give the same answer on every machine. The plan is written down. It's a big job, and I'd rather do it before the world fills up with things that would have to be moved.

## What this does not prove

The thrusters are tested by a script, not yet by me. The bigger world is a plan, not a single line of code yet. And I still don't know what the circles on the bridge floor are.

## Next dive

Either the big ocean or the hydrophones, whichever comes first. These are intentions, not promises.

*Beep. Bloop.*
