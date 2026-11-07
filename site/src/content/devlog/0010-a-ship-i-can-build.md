---
title: "A Ship I Can Build Myself"
date: 2026-11-07
summary: "Until now the whole ship was written in code, as a list of grey boxes. Now its look lives in the editor, where I can build rooms and a hull by hand, without breaking what makes walking on a moving ship work."
category: "Engineering"
tags: ["prototype", "art", "tools", "networking"]
status: "prototype"
---

Here's something you might not guess from the screenshots: until today, the entire ship was code. Every wall, floor, stair and console was a line in a C++ file, a box with a position and a size in centimetres. That's fine for testing, but it meant I couldn't open the editor and simply build a room.

## Now I can

The game now spawns the ship from a Blueprint, which is Unreal's way of saying "a thing you can open and arrange in the editor". All the ship's behaviour stays in code: moving, steering, the sonar, the stations, swimming, the networking. The Blueprint only says what it looks like. Anything I add to it moves with the ship and appears for every player.

The comfortable way to build a room is in an ordinary level. Then I package it into a single piece and drop it into the ship over the grey room it replaces. A switch hides all the grey boxes, so I can see my rooms on their own and spot the gaps.

## The rule that comes with it

Testing taught me one rule the hard way: everything you walk on aboard the ship has to be a single piece of collision. If a player stands on a separate piece and the connection drops some packets, other players see them metres away from where they really are.

So the ship enforces it. Anything I add is scenery: players walk through it rather than on it, and the ship's own collision checks and the sonar ignore it. That way the ship can't jam on its own furniture, and the sonar never maps its own hull. To test this, a solid crate gets attached to the ship in the middle of a corridor the test players walk along. They walked straight through it, and nothing needed correcting.

For now, walking still happens on the grey layout's floors, so my rooms have to line up with them. Next, a simple mesh of my own will take over as the walkable shell.

## What this does not prove

I haven't built a single real room yet. This is the workshop, not the furniture. The station spots, ladders and other gameplay spots are still numbers in code, and they need to become markers I can drag around in the editor too.

## Next dive

Make those gameplay spots draggable, and build a first real room to see how the whole process holds up. These are intentions, not promises.

*Beep. Bloop.*
