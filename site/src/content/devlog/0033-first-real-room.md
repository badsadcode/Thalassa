---
title: "The First Real Room"
date: 2026-11-09
summary: "The grey box starts coming off: the aft corridor is dressed with a sci-fi hallway kit, and the bridge gets a window you can actually see out of."
category: "Engineering"
tags: ["prototype", "ship", "art", "interiors"]
status: "prototype"
cover: "/images/devlog/0033-aft-corridor.jpg"
coverAlt: "The aft corridor dressed with sci-fi wall panels, floor plates, pipes and warm strip lamps."
---

Until now, the inside of the ship has been coloured grey boxes. They're good for testing whether people can walk, swim and stay in sync, but not much to look at. Tonight the first room got real walls.

![The aft corridor: panelled walls, floor plates, ceiling beams and strip lamps.](/Thalassa/images/devlog/0033-aft-corridor.jpg)

## Grey box underneath, art on top

The ship isn't rebuilt to do this. The grey layout stays as the skeleton. It decides where you can walk, where there's air and where the stations are, and it's what the multiplayer is tested on. The art goes on top, as scenery that you see but never bump into. Walls you actually hit are still the grey ones, so the art just has to sit on their faces.

I bought a sci-fi hallway kit on Fab. Luckily, its walls are exactly as tall as our deck, 3.5 metres. The aft corridor is now dressed with it: wall panels in three rows, trim along the top, columns, corners, door frames round each doorway, floor plates, ceiling beams, pipes and warm strip lamps. That's 284 pieces and eight lights. The room is an ordinary level that I can edit like any map. One command packs it into a single Blueprint the ship carries, and makes sure everything in it moves with the ship and never gets in a player's way.

![The doorway to the EVA room and airlock.](/Thalassa/images/devlog/0033-airlock-door.jpg)

The pieces were laid out by a script. It reads where the grey walls and doorways are and stretches the panels to fit between them. The script also works as a template, so the next room is mostly a matter of telling it where that room's walls are.

## A bigger window

The bridge window was a letterbox. Now it runs almost from floor to ceiling and wraps round the front of both sides, more like the panoramic glass in the concept art.

![The bridge, with the window now wrapping round the front.](/Thalassa/images/devlog/0033-bridge.jpg)

## What this does not prove

It's one room. The rest of the ship is still grey, and you can see the grey door jambs in the doorways. The lighting is a first guess.

One thing did go wrong, and the walking test caught it. On the second player's machine, the room arrived solid, so that player walked on the art floor, 6 cm above the real one. The host kept pulling them back down: 187 corrections on a walk that normally has one. The ship now switches art collision off wherever and whenever it shows up. The same walk is back to 1 correction.

*Beep. Bloop.*
