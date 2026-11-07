---
title: "The Sea Has a Floor, and a Ceiling"
date: 2026-11-07
summary: "Now that the ship can be steered, it needed to stop going through things. It can't fly out of the water any more, and the seabed stops it dead."
category: "Engineering"
tags: ["prototype", "helm", "testing"]
status: "prototype"
---

Once the ship could be steered, the obvious next question was: what happens if you steer it into the seabed? The answer was nothing. It went straight through. Point it upward and it would leave the sea entirely.

## The ceiling

The surface is now a limit. A surfaced ship floats with its middle about 15 metres down and can't rise any further. If you hold the planes on rise, it levels off instead of trying to climb out. The helm screen says **SURFACED**.

## The floor

The seabed, and anything else solid, now stops the ship dead. The ship weighs a great deal and doesn't bounce. The helm screen flashes **COLLISION**, and the game counts it, so it can do damage later.

The interesting part was the ship's length. It's 280 metres long, so tipping the nose down by 12 degrees drops the bow almost 30 metres. In the first test the ship never got close to the seabed, as far as its depth reading was concerned. It just tilted its nose into the ground. Turning and tipping are now checked too: if a turn would swing the hull into rock, the ship refuses to make it. Near the bottom, how the ship is tilted matters as much as how deep it is.

## How I tested it

The scripted helmsman got two new courses. In one, the ship starts 470 metres down, 30 metres off the seabed, and he dives at full speed. The bow hits after about eight seconds, and then he backs off: astern, nose up. In the other, the ship starts 60 metres down and he drives straight up until it surfaces.

Both worked, after two fixes. The first version counted one collision every frame for as long as the ship stayed pressed against the seabed, which added up to more than two thousand. It also made a pinned ship shudder, creeping forward a few centimetres and being pushed back out, twice a second.

## What this does not prove

A collision does nothing yet except stop the ship. No damage, no sound, no shake. Everyone aboard sees the ship stop, but a player sees it stop a fraction of a second after the host does, so the ship snaps back slightly on their screen. At a crawl on a fast connection that's a few tens of centimetres. At full speed on a slow connection it would be metres. A crash is a jolt anyway, but it should feel the same for everyone.

## Next dive

Make a collision feel like one: a bump everyone feels at the same moment, and a sound. These are intentions, not promises.

*Beep. Bloop.*
