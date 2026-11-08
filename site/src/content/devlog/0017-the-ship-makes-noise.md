---
title: "The Ship Makes Noise"
date: 2026-11-08
summary: "The ship stopped being silent: an engine room that hums and thumps, a hull that creaks more the deeper you go, water rushing past, bleeping consoles, and a horrible noise when you hit the seabed."
category: "Engineering"
tags: ["prototype", "sound", "atmosphere"]
status: "prototype"
---

The game is subtitled *Beep. Bloop.*, and until today it was nearly silent. Only the sonar pinged. A submarine full of machinery at the bottom of the sea should never be quiet, and the dread I want comes as much through the ears as through the eyes.

## What you hear now

- **The engine room** hums low and deep, with a pump thumping on its cycle and a whine that rises when the helmsman opens the throttle. It's loud in engineering and fades as you walk away. When the ship speeds up, the machinery takes a few seconds to wind up with it.
- **The hull**, wherever you are. Inside, there's a faint hum and a low rumble that never stops. Water rushes past, louder the faster the ship goes. And the steel creaks and groans under pressure: rarely near the surface, every few seconds in the deep, more when the ship turns. Out in the water as a diver, you hear the sea instead, and the water rushing past is much louder.
- **The consoles** bleep: rising tones when someone takes a station or switches something on, falling ones when they leave, a click when the helm changes mode, and an alarm when someone hits full stop.
- **Hitting the seabed** sounds like it should. A heavy thud, a crash, the hull ringing like a struck bell for a few seconds, and then a long groan as it settles.

## No sound files yet

Every one of these sounds is made by the game as it plays, from code, like the sonar ping was. There are no recordings yet. That makes them placeholders, like the grey deck, but they're already timed to what happens and they respond to depth, speed and the throttle. Real recordings can replace them later without changing how they behave.

## How I know they work without hearing them

The test machines can't listen, so the game can now record its own sound output to a file, and the file can be measured. In a test where the ship dives into the seabed, the loudest moment in the recording lands exactly when the bow hits, with the hull's ringing tones right where they should be. In the engine room, the machinery shows up clearly above the background hum.

That turned up two traps. Unreal mutes a game that doesn't have focus, and a test without graphics never moves its listener, so it can't hear sounds that come from a place. Both are written down now, so the next person doesn't spend an afternoon recording silence.

## What this does not prove

Nobody has listened to it in the game yet, me included. The console bleeps haven't been recorded at all. The impact sounds the same everywhere on the ship, where it should ring sharply near where you hit and arrive as a dull thump far away. Sound doesn't get muffled by walls yet. And it's all synthesised placeholders, not real sound design.

## Next dive

Listening outward: hydrophones, and the first things out there to hear. These are intentions, not promises.

*Beep. Bloop.*
