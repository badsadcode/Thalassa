---
title: "The Sonar Gets a Screen and a Voice, and the Ladder Was Never About Two People"
date: 2026-11-07
summary: "The sonar chart now lives on a screen on the bridge, remembers what it found, and pings. And the ladder bug from the last entry turned out to have a completely different cause."
category: "Engineering"
tags: ["prototype", "sonar", "sound", "networking", "testing"]
status: "prototype"
cover: "/images/devlog/0005-sonar-screen.jpg"
coverAlt: "In-engine view of a grey-box bridge: a dark console with a square screen above it, showing a sonar chart with range rings, the ship's marker in the middle and a short blue stripe of surveyed seabed. Diagnostic text overlays the top left."
---

The last entry ended with three intentions for the sonar: make the chart remember what it found, put it on a real screen, and give it a voice. All three happened.

## The chart remembers

The host saves the chart every 30 seconds and when the game closes, and loads it again on the next start. Anyone who joins later gets the whole chart, not just what was surveyed since they arrived. I tested the whole loop: survey, quit, start again, have someone join, and compare. Same chart, square for square.

## A screen on the bridge

Until now the chart only existed on the station view you get with Tab. It's now also on a screen above the sonar console, on the starboard side of the bridge, where anyone walking past can see it.

![The sonar screen above its console on the bridge, showing range rings, the ship's marker, and a short stripe of surveyed seabed ahead of the bow.](/Thalassa/images/devlog/0005-sonar-screen.jpg)

The screen and the Tab view are drawn by the same piece of code, so they can't disagree. Nothing new is sent over the network either: every player's game draws the screen from the chart everyone already shares.

It's still the debug chart, just on a flat panel. The sonar is supposed to end up looking high-tech and polished, with a visible sweep and fine detail. This isn't that yet.

## It pings

While the sonar is on, it pings every two seconds. Everyone aboard hears the same ping at the same moment, because it follows the host's clock. On the screen, a ring spreads out from the ship at the speed of sound in water, 1,500 metres per second, so it crosses the chart in about half a second.

The ping has no sound file. The game generates it as it plays: a tone that falls slightly in pitch and fades, followed by fainter, duller repeats, like the hull ringing. It's a placeholder, like the grey walls. But I've heard it in the game, and it sounds like a submarine.

## The ladder was never about two people

The last entry said two people arriving at a ladder together shoved each other away from it forever, and blamed the nudge that keeps crew from standing inside each other. That was wrong.

Logging every frame at the bottom of the ladder showed the real problem. While you walk around the ship, the game keeps your speed relative to the ship. Taking hold of a ladder let go of the ship for a moment, and the code then took the ship's speed away from yours a second time. On a ship doing 10 metres per second, anyone who reached a ladder was thrown backwards at about 6 metres per second, straight out of reach of it. Then they walked back and it happened again. One person or four, it didn't matter. What mattered was that the ship was moving, and in every test it was.

With that fixed, four test players all climbed the ladder, sometimes two at once. It also explained most of the position corrections in earlier tests on the deck: a test on a connection with about 135 ms of delay went from 122 corrections to 6.

## What this does not prove

Everything here was tested with several copies of the game on one PC. The new build hasn't been played on two real PCs, or over the internet. The airlock still has corrections on slow connections that this fix didn't touch.

## Next dive

The helm. Right now the ship is steered with test keys from anywhere aboard. It should be steered from the helm seat, by whoever is sitting there, and it should feel heavy. These are intentions, not promises.

*Beep. Bloop.*
