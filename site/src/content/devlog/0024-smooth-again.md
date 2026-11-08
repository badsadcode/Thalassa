---
title: "Smooth Again"
date: 2026-11-09
summary: "The ping caused frame drops. The culprit was the sonar chart, redrawing itself far too expensively, and now it's about eight times lighter."
category: "Engineering"
tags: ["prototype", "sonar", "performance"]
status: "prototype"
---

The ping looked and sounded good, but I got frame drops. Not a low frame rate all the time, but stutters.

## Finding it

Rather than guess, I added a timer that measures the worst frame every two seconds, and how long each suspect takes: the chart, the holotable, the ping's charting and the survey. The answer was clear: every time the sonar chart redrew its picture, it took about 77 milliseconds, up to ten times a second whenever the chart was on a screen. At 60 frames a second, a whole frame has about 16.

It was my own doing. With the ping, I made the chart cover the whole ocean and show a wider area. That meant more than twice the pixels, and every pixel looked its height up in a much slower way than before.

## Fixing it

- The chart now copies the cells it needs into a simple grid once, and every pixel reads from that.
- It draws fewer pixels: about as many as the screens can actually show.
- A slow colour conversion, done for every pixel, became a lookup table.
- The work is spread over several frames instead of all at once, and the screen keeps showing the last finished picture meanwhile.

With the chart and the holotable both in view, and a ping every eight seconds, the worst frame went from about 80 milliseconds to about 9.

## What this does not prove

I measured on my machine, in the open ocean, with one player. The timer only covers what I thought to time, and there may be other hitches out there. If you see one, the timer is now there to catch it.

*Beep. Bloop.*
