---
title: "Run, Stop, and Look Outside"
date: 2026-11-07
summary: "Three things I asked for after steering the ship myself: a way to sprint around it, a full stop button, and a station with cameras on the hull."
category: "Engineering"
tags: ["prototype", "helm", "movement", "stations", "networking"]
status: "prototype"
cover: "/images/devlog/0008-cameras.jpg"
coverAlt: "In-engine view across a grey-box bridge toward a console with two screens: a small one showing a grid of six camera views, and a larger one showing a single view of pillars rising from the seabed."
---

I finally steered the ship myself, and it feels nice: heavy, slow to get going, slow to stop. Walking around it and steering it gave me a list of things I wanted. Here are the first three.

## Sprinting

The ship is about 280 metres long, and walking from one end to the other takes a while. Hold Shift and you now run at twice walking speed.

That sounds like one line of code, and in a single-player game it would be. In a multiplayer game, your own game predicts where you're going before the host confirms it. If the two disagree about how fast you're allowed to move, the host keeps pulling you back. So sprinting is now part of every movement update your game sends, and the host runs exactly the same speed.

Testing it turned up a bug with ladders. A sprinter reaching a ladder crossed the whole zone where you can grab it in a fraction of a second, hit the wall behind it, and bounced off, again and again. Grabbing a ladder now slows you to climbing speed.

In the long run I want movement to feel smooth and fluid, like Titanfall: fast, keeping your momentum, with moves like a slide. Sprinting is the first step. Every move like that has to work on a ship that's moving under your feet, over a network, so each one needs the same testing walking did.

## Full stop

Sometimes you just need the ship to stop. Press X at the helm, or the big red button at the end of the helm console, which anyone on the bridge can press. The engines cut, every control goes to neutral, and the ship brakes until it's standing still, then stays where it stopped. From full speed, that takes about 15 seconds. It's a big ship.

The button lights up and the helm screen says FULL STOP until the person at the helm takes the controls again.

## A camera station

The bridge has a new station: the console beside the sonar. Six cameras sit on the outside of the hull, looking forward, back, to each side, straight down and straight up. A large screen shows one camera, and a smaller one shows all six at once. Step up to the station and press E, then A and D switch cameras, and Tab puts the chosen camera on your whole screen.

![The camera station across the bridge: a small screen with six camera views, and a larger one showing pillars rising from the seabed ahead of the ship.](/Thalassa/images/devlog/0008-cameras.jpg)

Everyone on the bridge sees which camera the operator picked. The pictures themselves aren't sent over the network. Each player's game draws its own, and only while someone there can actually see a screen, so the cameras cost nothing when nobody is looking.

The camera looking straight up shows black. There's nothing above the ship yet but empty sky. Deeper down, the cameras will need floodlights.

## A bigger idea behind it

The camera station is the first station built the same way as the helm: one person takes it, everyone can see its screens, and the same picture appears in the person's view with a key to make it bigger. Later stations, for engineering and the ship's other systems, will work the same way, each with its own monitors.

## What this does not prove

All tested on one PC, with scripted players. The full stop braked exactly as intended in a test, but I haven't pressed the red button myself yet. There's no slide, no wall running, nothing beyond sprinting.

## Next dive

More of that list: a slide, floodlights for the cameras, and more monitors. These are intentions, not promises.

*Beep. Bloop.*
