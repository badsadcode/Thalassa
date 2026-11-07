---
title: "Someone Has to Steer"
date: 2026-11-07
summary: "The ship is now steered from the helm, by whoever is standing at it, with a lever and a screen everyone can see. And it's heavy: it takes half a minute to get going and a long time to stop."
category: "Engineering"
tags: ["prototype", "helm", "networking", "testing"]
status: "prototype"
cover: "/images/devlog/0006-helm.jpg"
coverAlt: "In-engine view of the front of a grey-box bridge: a console in front of a wide window, with a screen to the left showing the helm's levers and readouts, and an amber throttle lever standing upright to the right."
---

Until now, anyone anywhere aboard could steer the ship with a few test keys. You could be in the engine room and turn the whole ship. That was only ever for testing, and now it's gone.

## The helm

The ship is steered from the helm, at the console by the bridge's forward window. Walk up, press E, and you're at the helm. The keys you walk with now drive the ship: throttle, rudder, and the planes that tip the nose up or down. Press E again to step away.

Only one person can be at the helm. The idea is that a crew decides who steers, and everyone else can see what they're doing.

![The front of the bridge: the helm screen to the left, showing throttle, rudder and planes as bars with speed, heading and depth, and the amber throttle lever to the right.](/Thalassa/images/devlog/0006-helm.jpg)

That's why the controls are physical. There's a throttle lever on the console that tips forward and back as it moves, and a screen beside it showing every control and what the ship is doing. Whoever is at the helm sees the same picture on their own screen. All of it shows the same state, so if you walk onto the bridge you can see at a glance what the helm is doing.

## Two ways to drive

- **Precise:** the ship only does something while you hold a key. Let go, and everything returns to neutral. It's for careful manoeuvring.
- **Set and go:** push the throttle to a setting and leave it, and the ship keeps going. It's for long trips, when the helm has better things to do than hold W.

The rudder centres itself when you let go, in both modes. Whether it should is an open question.

## It's heavy

I wanted the ship to feel big. It isn't a submarine simulator, but it shouldn't turn like a car either. At full throttle it takes about 20 seconds to reach 8 metres per second, and with the throttle off it takes nearly a minute to coast down to half speed. The rudder barely does anything until water is flowing past it. Going astern is weaker than going ahead.

These numbers are a first guess. Nobody has played with them yet, me included.

## How I tested it

A scripted helmsman joins the game as a second player, walks from where he spawns to the bridge, takes the helm, and drives a set course: full ahead, a turn each way, a dive and level off, all stop, then the same again in precise mode. He leaves the helm at the end. Every control he touched reached the ship, and his own position barely needed correcting, even on a connection with a quarter of a second of delay and 3% of packets lost.

## What this does not prove

It's all one PC again. The helm doesn't depend on any ship systems yet, because there aren't any. You stand at the helm rather than sit, and nothing stops the ship from driving into the seabed or out of the water.

## Next dive

Steer it myself, and change the numbers until it feels right. These are intentions, not promises.

*Beep. Bloop.*
