---
title: "Swimming Back to a Moving Submarine"
date: 2026-11-07
summary: "There is an airlock now. A scripted diver can flood it, swim out, let the ship carry on without them, catch up, and come home. Eight times in a row."
category: "Engineering"
tags: ["prototype", "airlock", "swimming", "networking"]
status: "prototype"
cover: "/images/devlog/0003-airlock.jpg"
coverAlt: "In-engine view from the corridor through the open inner airlock door, into a brightly lit yellow chamber with the closed outer door at the back. Diagnostic text overlays the top left."
---

While you're aboard, you move with the ship. Once you're swimming, the ship moves without you. That sounds simple until you stand in a flooded airlock, where you're swimming and still have to move with the ship.

## This week aboard Thalassa

There's now an airlock off the corridor. Press the panel and the inner door closes, the chamber floods, and the outer door opens. The two doors can never be open together, and a door won't close on someone standing in it.

Then there's a scripted diver that does the whole trip, over and over: into the chamber, flood, swim out 15 metres, stop and tread water while the ship carries on, chase it down, get back through the outer door, drain, and walk back into the corridor.

| Setup, ship moving up to 2 m/s | Ping | Complete trips | Stuck | Corrections | Visible pops |
|---|---|---|---|---|---|
| One diver | 16 ms | 8 | 0 | 18 | 0 |
| One diver, bad connection | 134 ms | 8 | 0 | 114 | 28 |
| One diver, worse connection | 238 ms | 8 | 0 | 211 | 33 |
| Four divers sharing the airlock | 15 ms | 7 each | 0 | 32–48 per client | 2–5 per client |

These are single runs, with scripted divers, on one PC.

## Behind the consoles

The water isn't a physics simulation. The hull has a list of compartments, in the ship's own coordinates, and everything outside them is sea. The airlock counts as water up to its current flood level. That one rule decides whether you're swimming, and it runs inside the same movement step that's predicted on your computer and checked on the server, so the two usually agree.

## What surprised us

On its very first attempt, the diver stood in the airlock forever. It had stopped just inside the door, within the door's safety margin, so the interlock correctly refused to close the door on it. A person could get stuck the same way with no idea why, so the screen now says DOORWAY BLOCKED.

The doors also caused most of the early corrections. A computer always hears that a door is opening slightly late, so its door is a little lower than the server's, and it bumps into a door the server says isn't there. Now a door stops being solid the moment it starts to open. That took corrections on a good connection from 55 to 18.

## What this does not prove

Corrections on a bad connection are still high, and they cluster at the outer door, right where you switch between moving with the ship and moving with the sea. Nobody has played this yet, so I can't tell you how it feels.

Swimming is also very much a placeholder. There's no suit, oxygen, pressure or water rendering, and the only sign you're underwater is a blue tint. At 2 m/s the ship is slow enough to catch, and nothing has tested what happens if it isn't.

## Next dive

Build a real packaged copy of the game and try it on two actual computers. Everything so far has been one PC talking to itself, and that needs to stop. These are intentions, not promises.

*Beep. Bloop.*
