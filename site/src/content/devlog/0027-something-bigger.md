---
title: "Something Bigger"
date: 2026-11-09
summary: "Small fish, big fish, giant jellyfish, glowing ones, and a 400-metre leviathan that comes to see who's pinging."
category: "Engineering"
tags: ["prototype", "creatures", "sonar"]
status: "prototype"
cover: "/images/devlog/0027-leviathan.jpg"
coverAlt: "The head of an enormous dark serpent-like creature rising out of murky water, a pale glowing eye and rows of blue-green lights along its body."
---

The gliders were alone down there. Not any more.

![The leviathan's head rising out of the dark, its eye and the lights along its flank glowing.](/Thalassa/images/devlog/0027-leviathan.jpg)

## Clutter fish

That's what I called them: lots of life that's just there, going about its business. Ten kinds now:

- **Darts**, small silver fish in schools of up to eighty.
- **Discs**, gold with dark bands.
- **Lanterns**, little dark fish with rows of glowing spots along their bellies.
- **Groupers** and **eels**, close to the ground.
- **Cruisers**, seven metres long, slow, with long fins and a faint glowing line down each side.
- **The gliders** from last time.
- **Giant jellyfish** with six-metre bells and tentacles trailing fourteen metres below.
- **Glowing jellyfish**, blue, with glowing rims and banded tentacles.
- **Motes**, swarms of tiny glowing jellyfish.

Like the rocks and the gliders, the seed decides where each school lives, and the game's clock decides where it is right now. So every player sees the same fish without anything being sent over the network. Some kinds prefer some places: lanterns and glowing jellyfish like the dark canyons and volcanic fields, groupers and eels the broken ground. The fish swim with a wave running down their bodies, and the jellyfish pulse.

![A big cruiser and glowing jellyfish over the seabed.](/Thalassa/images/devlog/0027-jellies.jpg)

When a ping's wave reaches them, fish bolt and scatter, and the glowing kinds flare.

![Looking down on gliders and a giant jellyfish, its rim glowing.](/Thalassa/images/devlog/0027-from-above.jpg)

## Something bigger

The design document says it should be possible to look out of a window and see only one enormous eye, with the rest of the creature lost in the dark. This is the first step towards that.

The leviathan is about 400 metres long. Our ship is 280. It has a long, segmented body tapering to an upright tail fin, a ridge of spines, rows of lights along its sides, and two glowing amber eyes, each over four metres across. It does a slow round about a kilometre and a half from the start.

Far off, you don't see it. You see a line of lights moving in the dark.

![At 500 metres, the leviathan is a line of lights in the dark.](/Thalassa/images/devlog/0027-lights.jpg)

And it can hear the ping. Ping from up to two kilometres away, and over the next couple of minutes it comes to find you. It circles where the ping came from, a few hundred metres out, then loses interest and leaves. So the ping maps the ground, scares the fish, and tells something very big where you are.

On the sonar it doesn't show as one contact but as a long curved chain of them. You might take it for several things in a row.

![The sonar chart, with the leviathan as a long curved chain of contacts to the north.](/Thalassa/images/devlog/0027-chain.jpg)

## What this does not prove

I tested it with two players, pinging every ten seconds. Both saw exactly the same contacts, even the player who joined late, which needed a fix: the game now shares the last ten minutes of pings, so a latecomer knows what everyone else's fish are reacting to. Walking and the ship stayed smooth, and the leviathan came from over two kilometres away to under one. That test ran before I made it 400 metres long, so it was still a smaller version.

What's missing: small fish are true to size, so in this murky deep water they're just dark specks beyond twenty or thirty metres; brighter shallower water will help. The leviathan doesn't avoid the ship yet (it can swim right through it), and it doesn't come up to the windows. Nothing makes a sound. That's where the hydrophone comes in: you should hear it long before you see it.

All placeholders, of course. Every creature is made from rules until we design the real ones.

*Beep. Bloop.*
