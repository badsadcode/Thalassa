---
title: "Spots I Can Drag"
date: 2026-11-08
summary: "The places the game cares about, like where the air is, where a ladder can be grabbed, and where the helmsman stands, are no longer numbers in code. They're markers in the ship's Blueprint that I can move with the mouse."
category: "Engineering"
tags: ["prototype", "tools", "art"]
status: "prototype"
---

Last time the ship's look moved into the editor, but some of the most important things on board were still numbers in a C++ file. The game doesn't only need to know what the ship looks like. It also needs to know a handful of places: where there's air and where there's sea, where you can grab a ladder, where each player appears, where you stand to take the helm, and where the airlock is.

## Now they're markers

Each of those places is now a marker inside the ship's Blueprint. The spaces with air are blue boxes. If you're inside one, you walk and move with the ship, and if you're outside all of them, you're in the sea and you swim. The ladder's climbing zone is a yellow box, spawn points are green figures the size of a crew member, and the helm and camera stations each have a purple one showing where their operator stands. The airlock is a single point, and its doors and water hang from it.

I can select any of them, drag it, resize it, and the game follows. I can also add more: if I build a new room, I give it a blue box and it has air. The markers are only visible in the editor. Nobody sees them in play and nothing bumps into them.

They start exactly where the old numbers were, so nothing on the ship has moved yet.

## How I know it works

With nothing moved, the usual tests came back the same as before: test players toured the whole deck with nothing corrected, the scripted helmsman drove the ship up to 14 m/s, and the diver still made it through the airlock on the small test rig. Then, in a throwaway copy of the ship, the first spawn point got dragged from the crew corridor to the bridge, and the first player appeared exactly there.

## What this does not prove

Dragging a marker doesn't drag the floor under it. Walking still happens on the grey boxes, so if I move the helm spot into thin air, the helmsman stands in thin air. The airlock's walls stay put too, and it can only face starboard for now. All of this changes when my own mesh takes over as the walkable shell.

And I still haven't built a real room. The workshop has more tools now, but there's still no furniture in it.

## Next dive

A walkable shell from a simple mesh of my own, so the floors, walls and markers can all move together. These are intentions, not promises.

*Beep. Bloop.*
