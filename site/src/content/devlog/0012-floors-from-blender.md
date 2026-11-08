---
title: "Floors From Blender"
date: 2026-11-08
summary: "What players walk on aboard the ship now comes from a Blender file instead of a list of boxes in code. The first attempt got people stuck on the steps; the second walks exactly like before."
category: "Engineering"
tags: ["prototype", "art", "tools", "networking"]
status: "prototype"
---

This morning, gameplay spots became markers I can drag around. One big thing was still locked in code, though: the floors, walls and ramps players actually walk on. Those were still the grey boxes from the C++ layout. Now they come from a Blender file.

## The walkable shell

There's a new file, `WalkableShell.blend`. Open it and you'll see the whole deck as boxes: every floor, wall, ramp and step, 179 of them, each its own object. Those boxes are what you walk on. I can move one, stretch it, tilt it into a ramp, or add a new one, then export, import into Unreal, and the ship walks on the new shape.

To check nothing gets lost on the way, every box was compared after the round trip from Unreal to Blender and back. The worst difference was 0.003 cm.

## The first try got people stuck

My first version did the obvious thing and used the mesh's own triangles as collision. The test players toured the ship as usual and got stuck six times, always climbing up a step: out of the sunken mess hall, and up the bridge's tiers.

The cause is hidden geometry. Where two boxes touch, their faces are still there inside the mesh, and the edges of those faces become little seams. Walking over them is fine, but stepping up onto a ledge, the capsule that stands for a player catches on a seam and stops.

So the shell's collision is simple boxes, one per box, which is what the grey layout always used. Blender names them `UBX_`, a prefix that tells Unreal "treat this as a box". With that, the test players went everywhere again. In one run at about 170 ms of ping, the client needed no corrections at all, though a single run is a small sample.

## What this does not prove

The shell is still exactly the old grey layout, just in a new home. I haven't changed a single floor yet, so the real test, me reshaping the ship in Blender, is still ahead. The markers from this morning don't follow the floors either, so if I move a deck, I'll have to move its markers too. I also haven't tried it in a packaged build yet.

## Next dive

Reshape something for real in Blender, and build a first actual room on top of the shell. These are intentions, not promises.

*Beep. Bloop.*
