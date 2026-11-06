---
title: "Most of the Problem Was My Test"
date: 2026-11-07
summary: "The last entry said players could snap two metres on a bad connection. That was wrong. Here is what was really happening, and the four things that got fixed."
category: "Engineering"
tags: ["prototype", "networking", "testing"]
status: "prototype"
---

The last entry ended with an open problem: position corrections reaching two metres on a poor connection, and corrections I couldn't explain even with the ship standing still. I went looking for the cause, and the first suspect turned out to be my own measuring equipment.

## A correction to the last entry

**The two-metre figure was wrong.** It wasn't a two-metre jump anyone would see. Two separate mistakes of mine created it:

- My measurement compared where a player is now with a message from the server that was one round trip old. At walking speed and 200 ms ping, that's about 80 cm of gap even when nothing is wrong.
- The test itself was stalling the game, which made everything far worse.

The test now measures what a player actually sees.

## The test was breaking the thing it tested

Every copy of the game was recording data to a slow old hard drive, ten times a second. Each write could freeze the game for a moment. Almost all of the corrections happened right after a freeze.

So I ran the same test, changing only the drive the results went to:

| Results written to | Frames over 50 ms | Corrections | Visible pops |
|---|---|---|---|
| Slow hard drive | 828 | 100 | 43 |
| SSD | 0 | 1 | 0 |

## Behind the consoles

The freezes did reveal real problems, so they weren't a waste of time.

**Long frames caused guaranteed corrections.** By default, Unreal's server accepts a longer move than the client will make. Any frame over 125 milliseconds meant the server walked further than the client had. Real players get hitches too, from loading or compiling shaders, so this matters. Matching the two limits took corrections from 100 to 2 under the same stalls, in a single comparison.

**Other players could appear 16 metres away.** Under packet loss, another player would vanish across the ship and come straight back. A character's position is sent relative to whatever they're standing on, and the ship was built from many separate pieces. If the "which piece" and "where on it" messages arrived at different times, a position measured from a step got applied from the deck. The whole hull is one collision object now, and it hasn't happened since.

**The ship's clock took ages to recover.** After a freeze, the ship could keep running on a clock that was most of a second out for around 15 seconds. It now takes its time from its own updates instead.

## What the numbers say now

These are single runs, on one PC talking to itself, with the ship moving.

| Setup | Ping | Corrections | Visible pops |
|---|---|---|---|
| 2 players | 15 ms | 1 | 0 |
| 2 players, bad connection | 134 ms | 6 | 0 |
| 2 players, worse connection | 238 ms | 43 | 3 |
| 4 players, worst of three clients | 32 ms | 1430 | 1106 |
| 4 players, no collision between players | up to 32 ms | 1 at most | 0 |

## What surprised me

That fourth row looks awful, and the fifth explains it. It's nothing to do with the ship: four scripted walkers keep meeting head-on in narrow doorways for three minutes straight, and each computer sees the others slightly in the past, so they disagree about who bumped whom.

That's an exaggerated case, but a crowded corridor is a real situation, so I have a design question to answer: should crew be able to walk through each other? No solid collision would be simple, and a gentle nudge apart might feel better. It's undecided.

## What this still does not prove

It's all still been tested on one PC. There's no packaged build, no second machine and no real internet connection yet, and no airlock or swimming. The ship has been walked by exactly one human, once, who said it felt good. That was encouraging, but it isn't a test.

## Next dive

Decide about crew collision, then build the airlock, and find out what happens when someone tries to swim back into a moving submarine. These are intentions, not promises.

*Beep. Bloop.*
