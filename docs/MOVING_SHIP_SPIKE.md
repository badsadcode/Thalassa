# Moving-ship spike: results so far

**Verdict: promising, not passed.** The Phase 1 gate in the design document is still open.

## What was tested

Approach A from design document section 19: a kinematic ship actor really moving through one persistent world, with the server publishing its pose and characters standing on it through `CharacterMovementComponent` based movement.

Each run has a listen host and one client as separate headless processes on one PC (UE 5.8.4, Development editor binaries, uncooked, `t.MaxFPS 60`). Both run a scripted walker that tours bridge, corridor and engineering steps, jumps every 9 seconds, and uses the console. The server drives a 185-second profile: rest, accelerate to 10 m/s, turn both ways at 3 deg/s, pitch and roll to 10 degrees and back, a hard manoeuvre at 25 m/s and 8 deg/s, then brake.

Run it with `python Scripts/build/run_motion_test.py`. Raw telemetry goes to `Saved/MotionTests/`, which is not committed.

## Results

Each row is one run of two players on a graybox ship about 40 m long.

| Run | Ship | Median ping | Corrections | Largest correction | Pops (largest) | Own pawn disagreement, median / max | Ship position disagreement, p95 / max | Attitude max |
|---|---|---|---|---|---|---|---|---|
| `control_static` | Still | 33 ms | 22 | 77 cm | 16 (48 cm) | 18 / 96 cm | 0 / 0 cm | 0 deg |
| `baseline` | Moving | 27 ms | 29 | 112 cm | 17 (61 cm) | 21 / 113 cm | 1.7 / 8.9 cm | 0.1 deg |
| `moving_lan` | Moving | 32 ms | 37 | 93 cm | 18 (51 cm) | 20 / 112 cm | 1.7 / 13.9 cm | 0.1 deg |
| `final_lan` | Moving | 27 ms | 46 | 100 cm | 30 (36 cm) | 26 / 260 cm | 68 / 136 cm | 0.4 deg |
| `final_lan2` | Moving | 30 ms | 49 | 100 cm | 28 (125 cm) | 21 / 99 cm | 1.9 / 22.9 cm | 0.2 deg |
| `moving_rtt100` | Moving | 207 ms | 120 | 203 cm | 42 (63 cm) | 88 / 214 cm | 1.3 / 9.4 cm | 0.2 deg |
| `moving_rtt200` | Moving | 268 ms | 86 | 173 cm | 21 (74 cm) | 111 / 189 cm | 1.8 / 8.8 cm | 0.1 deg |
| `moving_shipgravity` | Moving | 31 ms | 18 | 36 cm | 11 (22 cm) | 18 / 88 cm | 1.0 / 5.5 cm | 0.3 deg |

Across all of these runs: 0 frames outside the hull, 0 stuck waypoints, base never lost in any sample, and host and client agreed on the console count (22).

Notes on reading the table:

- **Use the measured ping, not the scenario name.** `rtt100` and `rtt200` are what was requested (with 10 ms / 1% and 20 ms / 3% jitter and loss). Measured ping came out much higher and not in proportion, and why has not been investigated.
- `final_lan` and `final_lan2` include interior lights and hull colours, which should not matter with rendering off. `moving_shipgravity` and earlier runs do not.
- Every row is a single run, and runs of the same setup differ a lot (29 to 49 corrections). Small differences mean nothing.

## What we learned

**1. The serious bug was ours, and is fixed.** The first runs showed 309 corrections, 125 pops, and the client's ship up to 161 degrees away from the host's. It only happened with negative rotation rates. `FRotator` net serialization wraps each axis into [0, 360), so a rate of -2 deg/s arrived as +358 deg/s and the client extrapolated from that. Rates are now sent as an `FVector`. The original metric only compared ship position, so attitude disagreement is now measured too.

**2. Ship motion is not the main source of remaining corrections, as far as we can tell.** With the ship held still, corrections were 22 against 29 to 49 moving, and they no longer cluster around manoeuvres. The cause has not been found. Untested candidates are the walkers bumping into each other, frame stalls, and jumps (the largest corrections tend to happen mid-jump).

**3. Latency hurts.** At about 200 ms and above, corrections reach about 2 m. That is not acceptable, and it is the main open problem. The rise in own-pawn disagreement is mostly expected, since a client is ahead of the server by roughly its ping times walking speed.

**4. Frame stalls desynchronise the ship clock for too long.** In `final_lan`, both processes hit 400 ms frames at the same moments, and the ship's client-side time stayed wrong for up to 20 seconds, worth about 1.3 m at 10 m/s. Those stalls were seen in both final runs and are probably from something else on the machine, though that is not confirmed. The bad recovery is ours either way: the clock smoothing in `AThalassaVessel::TickClient` should converge faster after a large step.

**5. Ship-relative gravity looks promising, on one run.** It had the fewest corrections and the smallest. It needs repeating, and it has not been tried with a real player: mouse look does not yet account for a tilted gravity direction.

**6. Headless tests cannot tell you the ship is visible.** The first rendered frames showed an almost black interior, because a sealed hull lets no light in. Keep capturing frames (`Scripts/build/capture_screenshots.py`).

## Not tested yet

The Phase 1 gate needs all of these:

- Approach B or C. The design document asks for a comparison, and approach A has not failed badly enough to rule them in or out.
- A person actually playing it. No human has walked this ship yet, so nothing here says how it feels, and it has never been measured with rendering on.
- Packaged builds, two real PCs, or any real internet path. These runs all use loopback.
- Airlock, swimming, re-entering a moving ship, ladders, doors, seated stations, loose objects.
- More than two players. Save and load in motion, joining mid-voyage.

## Next steps

1. A person plays it in the editor with two players, to find out how it feels.
2. Find what causes corrections with the ship still, starting with one walker alone.
3. Fix clock recovery after a stall.
4. Repeat the ship-gravity run, and make mouse look work with it.
5. Airlock and swimming, the next item on the gate.
