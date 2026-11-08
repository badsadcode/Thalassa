# Helm: first version

The ship is now steered from the helm, by whoever is standing at it. The old test keys, which steered from anywhere aboard, are gone. The owner's direction for the helm is in `Decisions.md`.

## Using it

Walk to the middle of the console by the bridge's forward window and press **E**. Your character steps onto the helm spot and stays there, and the movement keys now drive the ship:

| Keys | At the helm |
|---|---|
| W / S | Throttle ahead / astern |
| A / D | Rudder port / starboard |
| Space / C | Vertical thrusters: up / down. The ship rises or sinks level. |
| Q / Z | Planes: bow up (rise) / bow down (dive) |
| B | **Test boost** (test and development builds only): ×10 top speed and vertical thrust, for crossing the open ocean while testing. Shown on the helm screen. A shipped game has no boost; upgrades may make the ship faster later. |
| R | Switch between precise and set-and-go |
| X | Full stop: engines cut, and the ship brakes to a standstill and holds there |
| E | Stand up |

Mouse look still works. Only one person can be at the helm, and anyone pushed more than 3 m from it, or who disconnects, leaves it.

- **Precise:** the ship answers only while you hold a key. Let go, and the levers return to neutral.
- **Set and go:** holding W or S moves the throttle up or down, and it stays where you leave it. Space and C do the same for the vertical thrusters, and Q and Z for the planes. The rudder always centres itself when you let go, in both modes.

Switching mode never lurches the ship: the new mode starts from wherever the levers are.

**Full stop** is X at the helm, or the red button at the starboard end of the helm console, which anyone within reach can press with E. Every lever goes to neutral, and the ship brakes at 1 m/s per second, about 15 seconds from top speed. Turning and pitch stop too, and the ship stays where it stopped. The button lights up, and the helm screen says FULL STOP. It lasts until the helmsman touches a control again. In a test at 14.4 m/s, the ship lost exactly 1 m/s each second until the helmsman took the controls back, and it then answered them.

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
- **Vertical thrusters (2026-11-08, owner's request)** lift or sink the ship level, without tipping it. They reach 1.5 m/s after about 12 s, and the ship keeps drifting for a while after you let go. They can't lift the ship past the surface, contact with rock or the surface stops the drift, and full stop brings them to rest. The helm screen shows THRUST.

The numbers are constants at the top of `ThalassaVessel.cpp`, gathered so upgrades can change them later. The scripted motion test still uses its own fixed profile, so older results stay comparable.

## The surface and the seabed

Under the helm, the ship can't leave the water or pass through the world:

- **The surface** is world Z = 0. Surfaced, the ship floats with its origin 15 m below it. It can't rise further, and the planes can't pitch it up while it's there. The helm shows **SURFACED**.
- **The seabed, and anything else solid,** stops the ship dead. Each tick the server sweeps the hull's box along the ship's move, against world geometry only, so a swimmer never stops the ship. A turn or pitch that would swing the hull into rock is refused first. On a 280 m hull, 12 degrees of pitch moves the bow 29 m, so near the seabed the pitch matters as much as the depth. The helm flashes **COLLISION** for 4 seconds, and the count is replicated, ready for damage later.
- **Backing off works:** a move away from the contact is allowed. Astern with the planes to rise frees a ship pinned bow-down.

The scripted motion test is not constrained, so its path, and earlier results, stay the same.

The ship stops on the server at once, but a client is still predicting it forward for a moment, so it sees the ship stop slightly late and snap back. At LAN ping and 2 m/s that was 36 cm. Faster, or at a high ping, it will be more. A collision is a jolt anyway, but a bump that plays the same on every machine would be better.

`--helm-course dive` and `--helm-course rise` drive the ship into the seabed and up to the surface. `--ship-at X,Y,Z` starts it somewhere else, in world centimetres. The test map's seabed is at 500 m, with pillars standing up to 360 m. `--ship-at 0,5000,-47000` starts 470 m down in a lane between the pillars.

## Tested

`python Scripts/build/run_motion_test.py --name helm --helm [--clients 2] [--rtt 200 --jitter 20 --loss 3]` runs the deck with a scripted helmsman on the first client. He walks from the spawn to the bridge, takes the helm, and drives a course: full ahead in set-and-go, a turn each way, a dive and level off, all stop, then precise mode, and stands up. His log lines start `HELMBOT`.

| Run (one each) | Median ping | Helmsman's corrections | Ship disagreement, max |
|---|---|---|---|
| `helm_lan`, 2 players | about 26 ms | 0 | 1.2 cm |
| `helm_lag`, 3 players | about 240 ms, 3% loss | 5, largest 1.8 cm | 7.7 cm |

In both, every lever, mode change and stand-up reached the ship as scripted, and the ship's speed, heading and depth followed.

The limits, at LAN ping:

| Run | What happened |
|---|---|
| `helm_dive`, starting at 470 m | Full ahead, full dive. The bow met the seabed 8 seconds in, at 10 degrees of pitch: one collision, and the ship held still against it with no shudder. Astern with the planes to rise backed it off. No corrections. |
| `helm_rise`, starting at 60 m | Full ahead, full rise. The ship reached 15 m after 40 seconds, levelled off and stayed there. No collisions, no corrections. |

Two faults were found and fixed along the way. The collision count went up every frame while the hull stayed pressed against the seabed, and a pinned ship shuddered by 4 cm twice a second, creeping in and being pulled back out. A frame confirmed the helm screen shows SURFACED. The COLLISION warning has not been seen in a frame. Frames confirmed the screen is the right way up and the lever is visible. The front of the bridge was too dark to see either, so it got its own lamp.

| `heave` (2026-11-08) | Zero throttle. Thrusters down for 25 s, let go, thrusters up for 30 s, let go. The ship sank from 200 m to 237.5 m and rose to 192.5 m, reaching 1.5 m/s and drifting on after each release, with the pitch at exactly 0.0 throughout. No corrections for the walkers aboard. |

## Not done yet

- **Ship systems.** The direction is that steering depends on the ship's systems working. There are no systems yet, so the helm always works.
- **A seat.** You stand at the helm. A real seat, and an animation, can replace the spot you walk onto.
- **A rudder and planes you can see.** Only the throttle has a physical lever.
- **Collisions do nothing yet** but stop the ship. No damage, no sound, no shake.
- **Feel.** The numbers are a first guess and haven't been tuned by anyone playing.
