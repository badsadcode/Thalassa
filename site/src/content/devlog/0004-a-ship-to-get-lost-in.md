---
title: "A Ship to Get Lost In, and a Sonar to Find Your Way"
date: 2026-11-07
summary: "The test ship became a deck you can explore: a ring corridor, a sunken mess hall, a tiered bridge and an engineering hall with a ladder. The ship also got its first sense, a sonar that maps the seabed it actually detects."
category: "Engineering"
tags: ["prototype", "layout", "sonar", "networking"]
status: "prototype"
cover: "/images/devlog/0004-bridge.jpg"
coverAlt: "In-engine view across a grey-box bridge: tiered floors stepping down toward a wide window looking out into blue water, with dark console blocks along the sides. Diagnostic text overlays the top left."
---

Until now the ship was three grey rooms in a row. That's fine for testing, but it's exactly what THALASSA shouldn't be: a cigar with `[BRIDGE][ROOM][AIRLOCK][ROOM]` in a line.

## What the ship should feel like

The direction for the ship is somewhere between a starship and a submarine. It shouldn't be cramped, but it shouldn't be a floating city either. The working size is about 280 metres, roughly the length of the exploration ships many of us grew up watching on television. It's a number to build toward, and it'll change.

The bigger rule is about shape. Corridors should loop, so there's more than one way to get anywhere. Rooms should sit off corridors in different sizes, and the floors shouldn't all be level: a few steps down into the mess hall, a bridge that drops in tiers toward the window. And at first, you should get a little lost.

## This week aboard Thalassa

So there's now a first deck to walk around. It's still grey boxes, and it's still a sketch:

- **A ring corridor**, deliberately off-centre, around a **sunken mess hall** two steps down.
- A **lounge** in a blister on the port side, with a long window out to sea.
- A **tiered bridge** at the bow, stepping down toward its forward window.
- An **engineering hall** four metres lower than everything else, reached by stairs on one side and a ladder on the other.
- The **airlock**, moved to the outer hull.

![The sunken mess hall, seen from its upper edge: a step down into a lower floor with low tables.](/Thalassa/images/devlog/0004-mess.jpg)

![The engineering hall from its floor: a large machinery block, and a staircase climbing to a catwalk.](/Thalassa/images/devlog/0004-engineering.jpg)

## Behind the consoles

The ship's first sense is a **sonar**. Switch it on, and a fan of beams sweeps the seabed below and beside the ship, ten times a second, out to 800 metres. Every return fills a 25-metre square of a chart: dark blue for deep water, pale cyan for shallow. Anywhere nobody has measured stays blank. The chart isn't a map you're handed. It's the map you made.

Everyone aboard shares the same chart. In testing, the host and a joining player ended a three-minute survey with identical charts, down to the last square, even on a connection losing 3% of its packets.

![The station view: ship status on the left, and on the right the sonar chart, a single blue stripe of seabed just ahead of the ship's marker.](/Thalassa/images/devlog/0004-sonar.jpg)

That picture is from a ship standing still, so it's just one sweep. Get moving, and the stripes add up to a strip of charted seabed behind you.

## What surprised me

**Stairs are lies.** In most games, the steps you see aren't what you walk on. There's an invisible ramp underneath, because walking down real steps is a series of tiny falls. Now this game has invisible ramps too.

**Being polite is hard.** The gentle nudge that stops crew standing inside each other also pushed people off ladders. Two people arriving at the bottom of a ladder together shoved each other away from it forever. The nudge now switches off while you're climbing. It still needs to switch off at the bottom of a ladder too.

**From the very first commit, none of the test scripts were actually saved to the project's history.** A rule meant to ignore one folder quietly ignored another with a similar name. They're there now.

## What this does not prove

The deck is a sketch, and none of its geometry will survive. Real interiors will replace it, which is why I stopped polishing it. What does survive is a list of rules any real interior has to follow, each one learned from a test that failed.

There's now a packaged copy of the game that runs without the editor, and two copies of it can play together. But they've only played together on one PC. Playing over the internet isn't set up yet, and hosting from behind a shared connection, like mine, needs extra help.

## Next dive

Make the chart remember what it found after you close the game, put it on a real console screen, and give the sonar its voice: a ping, and eventually a hydrophone to listen with. These are intentions, not promises.

*Beep. Bloop.*
