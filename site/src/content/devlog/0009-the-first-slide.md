---
title: "The First Slide"
date: 2026-11-07
summary: "A first step toward fast, fluid movement: crouching, and a slide out of a sprint. Plus a way for my AI assistant to work in the Unreal editor itself."
category: "Engineering"
tags: ["prototype", "movement", "networking", "tools"]
status: "prototype"
---

I want moving around THALASSA to feel smooth and fluid in the end, something like Titanfall: fast, keeping your momentum, never fighting the controls. That's a long way off, but the first piece is in.

## Crouch, slide, jump

C crouches. Sprint, then press C, and you slide: you keep the speed you had, lose it slowly over a second and a half, and can steer a little. A slide out of a sprint carries you about ten metres. Press Space during the slide and you jump out of it with its speed. The game used to refuse to let you jump while crouched at all.

Crouching used to snap your view down a third of a metre in a single frame, because the camera was fixed to your body and your body got shorter at once. The view now eases down and back up.

## The network part

As with sprinting, the hard part is multiplayer. Your game predicts your movement, and the host checks it. If they disagree about whether you're sliding, the host pulls you back.

So there's no slide timer and no burst of speed when a slide starts, because those are extra things the two sides could disagree about. Whether you're sliding follows only from how fast you're going and whether you're crouching, and the game already keeps both in agreement. The first version also let your game merge several small movement steps into one before sending them. A slide steers and slows a little each frame, so one long step doesn't end up quite where several short ones do. Sliding steps now go one at a time.

Test players sliding every six seconds on a connection with about an eighth of a second of delay had one visible correction in three minutes, of 11 cm. Without that last change it was four, the biggest 27 cm.

## A new tool: Unreal MCP

So far my AI assistant has worked blind. It writes code, runs the game in the background with scripted players, and reads screenshots. That's fine for systems like these, but it'll be a poor way to dress the ship in real art.

Unreal Engine 5.8 ships an experimental plugin from Epic that lets an assistant work in the open editor itself: placing and changing things while I watch. It's turned on now, for the editor only and only on my own PC. The next big job is making the ship something I can build in the editor rather than in code, so that should help.

## What this does not prove

All one PC, all scripted players. There's no mantling, no wall running, no boosted slide yet, and the slide's numbers haven't been tuned by anyone actually playing. The editor plugin is installed but not yet used for real work.

## Next dive

Turn the ship into something I can build myself in the editor, rooms and hull, without breaking the rules that keep movement on a moving ship working. These are intentions, not promises.

*Beep. Bloop.*
