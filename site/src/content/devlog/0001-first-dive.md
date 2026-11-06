---
title: "First Dive: The Ship That Turned the Wrong Way"
date: 2026-11-06
summary: "Before there is an ocean, a crew has to be able to walk around inside a moving submarine over a network. Here is the first test of that, the bug it found, and what is still not solved."
category: "Engineering"
tags: ["prototype", "unreal", "networking", "submarine"]
status: "prototype"
cover: "/images/devlog/0001-corridor.jpg"
coverAlt: "In-engine view from the graybox bridge, looking aft through a doorway and down a long corridor toward orange-lit steps in engineering. Diagnostic text overlays the top left."
---

THALASSA is meant to be a game about living aboard a submarine, with up to eight people walking around inside it while it moves. That sounds like a given until you try it. Every player is standing on something that is itself moving and turning, on several computers that all disagree slightly about where it is.

If that doesn't work, nothing else matters, so it comes first: no ocean, no creatures, no art. Just coloured boxes.

## What exists

A ship about 40 metres long, with a bridge, a corridor, an engineering space with some steps, and an opening for an observation window. There's a console that counts button presses, and the count reaches every player. That's the whole game so far.

![The graybox bridge: a plain dark console in front of a teal wall, with a readout showing beep count, depth, speed and heading.](/Thalassa/images/devlog/0001-console.jpg)

There is also a test. One copy of the game hosts and another joins, and a scripted walker in each one tours the ship while the server puts it through a set routine: accelerate, turn, pitch, roll, a hard manoeuvre, then brake. Each copy records what it believes is happening, and a script compares the two.

## Behind the consoles

The first useful run looked terrible: 309 position corrections on the client, the largest over two and a half metres. But the pattern was odd. Turning to starboard was fine, and turning to port was a mess. Pitching up was fine, and levelling off wasn't.

Physics doesn't care which way you turn, but a number format might. Unreal's rotation type wraps every angle into the range 0 to 360 when it goes over the network. That's fine for a direction, but I was also using it to send how *fast* the ship was turning. So a turn of -2 degrees per second arrived as +358 degrees per second.

Once I measured it, the joining player's ship was up to 161 degrees away from the host's. Worse, my test had reported the ships in nearly perfect agreement, because it only compared their positions. Both copies had the ship in the right place. One of them just had it pointing the wrong way.

The fix took a few lines. The test now also compares which way the ship is pointing.

## What the numbers say now

> **Correction, 7 November:** the correction sizes below, and the two-metre claim, were wrong. The test was stalling the game, and the measurement exaggerated what a player would see. See [the next entry](/Thalassa/devlog/0002-the-bug-was-my-test/). The original text is left as written.

These are single runs, and runs of the same setup vary quite a bit, so please don't read much into small differences.

| Setup | Ping | Corrections | Largest |
|---|---|---|---|
| Ship held still | 33 ms | 22 | 77 cm |
| Ship moving (four runs) | 27–32 ms | 29–49 | 93–112 cm |
| Ship moving, bad connection | 207 ms | 120 | 203 cm |
| Ship moving, worse connection | 268 ms | 86 | 173 cm |

The good news: across all of these, nobody fell through the hull or ended up outside it, and everyone agreed on the button count. The two copies also stayed within a fraction of a degree of each other on the ship's attitude.

The bad news: a two-metre correction on a poor connection means your character visibly jumps, and that isn't good enough. There are also corrections with the ship standing still, and I don't yet know why.

## What surprised us

The ship's interior was pitch black the first time I actually looked at it. All those careful measurements came from copies of the game running with no graphics, and none of them could tell me that a sealed metal box has no light in it. It has lamps now.

![The observation window opening, looking out from the bridge into open blue-green water.](/Thalassa/images/devlog/0001-window.jpg)

## What this does not prove

So far this has only run on one PC talking to itself, with scripted walkers. No person has played it, so I can't tell you how it feels. There's no airlock and no swimming yet, and it has never been tested with more than two players or over the real internet. Those are all still between here and calling this problem solved.

## Next dive

Walk around in it as an actual person. Find out what's causing those corrections on a still ship. Then build an airlock and see what happens when someone tries to swim back into a moving submarine. These are intentions, not promises.

*Beep. Bloop.*
