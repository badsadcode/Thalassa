# Helm: first version

The ship is now steered from the helm, by whoever is standing at it. The old test keys, which steered from anywhere aboard, are gone. The owner's direction for the helm is in `Decisions.md`.

## Using it

Walk to the middle of the console by the bridge's forward window and press **E**. Your character steps onto the helm spot and stays there, and the movement keys now drive the ship:

| Keys | At the helm |
|---|---|
| W / S | Throttle ahead / astern |
| A / D | Rudder port / starboard |
| Space / C | Planes: rise / dive |
| R | Switch between precise and set-and-go |
| X | All stop: throttle and planes to neutral |
| E | Stand up |

Mouse look still works. Only one person can be at the helm, and anyone pushed more than 3 m from it, or who disconnects, leaves it.

- **Precise:** the ship answers only while you hold a key. Let go, and the levers return to neutral.
- **Set and go:** holding W or S moves the throttle up or down, and it stays where you leave it. Space and C do the same for the planes. The rudder always centres itself when you let go, in both modes.

Switching mode never lurches the ship: the new mode starts from wherever the levers are.

## What everyone sees

The levers are the helm's state. They move at a lever's speed, never jumping, and they are replicated, so everyone aboard sees the same thing:

- **The throttle lever** on the console, to starboard of the helm, tips forward for ahead and back for astern.
- **The helm screen**, to port, so the view ahead stays clear. It shows the mode, who is at the helm, the three levers as bars, and speed, heading, depth and pitch.
- **The HUD** at the helm shows the same picture as the screen, drawn by the same function (`ThalassaHelmDisplay::Draw`), with the keys listed under it.

## How the ship moves

The ship is heavy, though this isn't a submarine simulator. Thrust works against water resistance:

- Full throttle reaches about 8 m/s in 20 seconds and 12.5 m/s in 40, toward a top speed of 15 m/s (29 knots).
- With the throttle at neutral the ship coasts: from 14 m/s to 7 m/s takes about 50 seconds.
- Astern is weaker, topping out around 5 m/s.
- The rudder needs water flowing past it: full turning rate, 4 degrees per second, from 6 m/s. Below that it fades, down to 15% at rest. Going astern, the rudder turns the other way.
- The planes pitch the ship up to 12 degrees, so the ship changes depth by moving along its own nose.

The numbers are constants at the top of `ThalassaVessel.cpp`, gathered so upgrades can change them later. The scripted motion test still uses its own fixed profile, so older results stay comparable.

## Tested

`python Scripts/build/run_motion_test.py --name helm --helm [--clients 2] [--rtt 200 --jitter 20 --loss 3]` runs the deck with a scripted helmsman on the first client. He walks from the spawn to the bridge, takes the helm, and drives a course: full ahead in set-and-go, a turn each way, a dive and level off, all stop, then precise mode, and stands up. His log lines start `HELMBOT`.

| Run (one each) | Median ping | Helmsman's corrections | Ship disagreement, max |
|---|---|---|---|
| `helm_lan`, 2 players | about 26 ms | 0 | 1.2 cm |
| `helm_lag`, 3 players | about 240 ms, 3% loss | 5, largest 1.8 cm | 7.7 cm |

In both, every lever, mode change and stand-up reached the ship as scripted, and the ship's speed, heading and depth followed. Frames confirmed the screen is the right way up and the lever is visible. The front of the bridge was too dark to see either, so it got its own lamp.

## Not done yet

- **Ship systems.** The direction is that steering depends on the ship's systems working. There are no systems yet, so the helm always works.
- **A seat.** You stand at the helm. A real seat, and an animation, can replace the spot you walk onto.
- **A rudder and planes you can see.** Only the throttle has a physical lever.
- **The surface and the seabed.** Nothing stops the ship rising out of the water or diving into the seabed.
- **Feel.** The numbers are a first guess and haven't been tuned by anyone playing.
