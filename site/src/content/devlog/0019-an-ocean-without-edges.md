---
title: "An Ocean Without Edges"
date: 2026-11-08
summary: "The seabed is no longer a six-kilometre model. It's generated from a single number, as you go, the same on every machine, with no edge in sight."
category: "Engineering"
tags: ["prototype", "world", "procedural", "networking", "sonar"]
status: "prototype"
---

The test ocean was six kilometres across, and I could feel its edges. So I started on the real one, the way I do in my other game: build the world procedurally first, make sure the terrain is believable, and only then worry about placing things by hand.

## One number

The new ocean comes from a seed: one number. From it the game works out the height of the seabed at any point you ask about:

- abyssal plains two and a half kilometres down, rising to shelves around four hundred metres;
- long, sharp-crested ridges;
- canyons cut down the shelves;
- lone seamounts, some over two kilometres tall;
- rolling hills, and fine detail under the floodlights.

There's no model of the seabed anywhere. The game builds the ground in tiles around you as you go, fine near you and coarser further out, and throws away what you've left behind. It doesn't end.

## The same ocean for everyone

Every player's game builds its own ocean from the seed, so the ground is never sent over the network. That only works if every machine gets exactly the same answer, down to the centimetre, so the generator avoids anything that can differ between computers, even the standard sine function. At start-up the game prints a fingerprint of the seabed. So far it has been identical in every game, on the host and the client alike.

The host also builds solid ground around every player and around the ship, wherever they are, so a ship sinking onto the seabed stops on it. In the test, it settled at 941 metres onto ground at 946.

## A sonar that reaches

My first try with the sonar in the new ocean charted nothing at all. It worked fine, but its range was 800 metres, and this ocean is often more than a kilometre deep. Real survey sonars reach several kilometres, so now this one does too. And because it reads the seabed's shape straight from the generator, it can chart ground far beyond what's been built around the ship.

## What it looks like

At the depths the floodlights reach, it looks like a believable, gently rolling seabed fading into the dark. The big features are hundreds of metres to kilometres across, far more than you can ever see at once down there. You'll meet them through the sonar and the holotable first, and then with your floodlights on their flanks. That suits me fine.

## What this does not prove

It's terrain only. No rocks, formations or life yet, on purpose. The sonar chart can still only hold the six kilometres around the starting point, so it stays empty further out. The terrain is believable, but not yet interesting up close. A thin line shows along one tile edge. I haven't measured how it performs with players far apart, and as always, it hasn't been tested on two real computers.

## Next dive

A sonar chart that grows with the ocean, and making the terrain more characterful up close. These are intentions, not promises.

*Beep. Bloop.*
