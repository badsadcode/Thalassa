---
title: "Pulse"
date: 2026-11-09
summary: "Jellyfish that swim in strokes and glow with every beat, in six colours or none at all. And krakens."
category: "Engineering"
tags: ["prototype", "creatures"]
status: "prototype"
cover: "/images/devlog/0028-kraken.jpg"
coverAlt: "Looking up at a giant squid-like kraken hanging in murky water, its arms trailing, with deep-red jellyfish and a pale giant jellyfish beside it."
---

I wanted a swarm of glowing jellyfish pulsing through the dark. So I made one, and while I was at it, a kraken.

![Looking up at a kraken, its arms trailing, with red jellyfish drifting beside it.](/Thalassa/images/devlog/0028-kraken.jpg)

## Swimming in strokes

Jellyfish don't just float. Their bells draw in and relax, and each stroke pushes them up a little before they sink back. Now ours do that: the bell contracts, the jellyfish bobs up, and the stroke ripples down its tentacles. Small ones beat quickly, giant ones slowly.

And the glow follows the beat: brightest as the bell draws in, dimming as it relaxes. Each jellyfish has its own rhythm, so a swarm doesn't blink together like Christmas lights. It shimmers.

The game's clock drives the rhythm, so every player sees the same jellyfish on the same beat.

![Amber glowing jellyfish drifting above gliders.](/Thalassa/images/devlog/0028-amber.jpg)

## Colours, and no colour at all

Each school now picks its colour: glowing blue, violet, green, amber, pink or cyan. Some don't glow at all: pale, nearly clear, deep red, or dusky purple. That last part comes from real deep-sea animals. Red light doesn't reach the deep, so red looks black down there, and it's one of the best ways to hide.

There are two new kinds, too. Helmet jellyfish have tall bells, and either a glowing ring or none. Comb jellies are egg-shaped, with eight glowing rows.

![Deep-red helmet jellyfish that don't glow, and a pale giant jellyfish.](/Thalassa/images/devlog/0028-red.jpg)

![A drift of comb jellies, glowing faintly violet.](/Thalassa/images/devlog/0028-combs.jpg)

## Krakens

Giant squid-like animals, 55 to 90 metres from the tip of the body to the end of the two long tentacles. They're dark red-brown with pale spots, and have big, faintly glowing eyes. They swim body first, with slow strokes, about one every ten seconds. Each stroke surges them forward while their arms close together and spread again.

Ping near one and it bolts, its spots flaring. On the sonar, a kraken is one big contact. They're rare, mostly in the canyons, ridges and volcanic fields, but there's one near the start, so you don't have to go looking.

## What this does not prove

I ran the two-player test again with all of it: same contacts on both machines for every ping, identical sonar charts, and walking and the ship as smooth as before. It all costs under a millisecond a frame. I haven't yet sat in the dark watching a big glowing swarm drift past the window, which is the real test, and the swarms you see here were gathered round one spot for the pictures. Out in the ocean they're spread out, and the small ones are hard to see in the murk until they come near your lights.

*Beep. Bloop.*
