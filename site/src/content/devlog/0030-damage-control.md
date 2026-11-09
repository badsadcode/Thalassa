---
title: "Damage Control"
date: 2026-11-09
summary: "Phase one is a leviathan hunt, and that starts with a ship that can get hurt: broken systems, breaches, rising water, and repairs together."
category: "Engineering"
tags: ["prototype", "ship", "co-op"]
status: "prototype"
cover: "/images/devlog/0030-alarm.jpg"
coverAlt: "The ship's interior in pulsing red alarm light, with a damage control list of broken systems, breaches and flooding."
---

Building all this life in the ocean, I kept imagining the moment something big decides the ship is a problem. So here's the plan for phase one: **a leviathan hunt.** Find it, track it, fight it, while it fights back. And for that, the ship has to be able to get hurt.

![The deck in red alarm light, with the damage control panel listing what's broken and what's flooding.](/Thalassa/images/devlog/0030-alarm.jpg)

## What can break

Five systems, each with its own health:

- **Engines:** less thrust, down to a limp.
- **Steering:** slower turns, lazier dives.
- **Sonar:** badly damaged, it stops pinging and mapping altogether.
- **Lights:** the floodlights outside dim and stutter, then go dark.
- **Pumps:** they're what fights the water, so when they're hurt everything else gets worse.

Each one lives somewhere on the ship. The engines and steering gear are in the engine room, the pumps aft, the light panel midships, and the sonar on the bridge.

## Holes in the hull

A hard enough hit tears a breach in the nearest compartment, and water comes in. That water is real: it rises, and once it's over your head you're swimming inside your own ship. The pumps fight it, if they still work. While the hull is breached, the lights turn red and pulse.

![Water rising in the engine room under the red alarm lights.](/Thalassa/images/devlog/0030-flooding.jpg)

![A breach in the hull: a jet of water bursting in through the wall.](/Thalassa/images/devlog/0030-breach.jpg)

## Fixing it, together

Walk up to anything broken and press E to start repairing it. Stay close and it slowly comes back. Two of you on the same breach close it twice as fast. That's the co-op heart of this: one person at the helm, one at the sonar, two people in the engine room up to their knees in water, trying to get the pumps going.

For now, the ship gets hurt when it crashes into the seabed or a rock, and harder the faster it hits. There's also a test key that knocks it about at random. The leviathan comes next.

## What this does not prove

I tested it with two players, knocking the ship every twelve seconds: both saw the same damage, the same breaches and the same water. Walking stayed smooth, until the ship flooded so badly that the test's walkers had to swim. What I haven't tested yet is a real crash at the helm doing the damage, or fixing a breach by hand. The visuals are placeholders too: the "jet of water" is a glowing cone.

*Beep. Bloop.*
