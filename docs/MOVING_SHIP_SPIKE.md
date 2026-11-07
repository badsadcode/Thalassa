# Moving-ship spike: results so far

**Verdict: walking on a moving ship, and leaving and re-entering it through an airlock, work in every test so far. The Phase 1 gate is still open**, because everything has run on one PC, in editor builds, mostly with scripted players.

## What was tested

Approach A from design document section 19: a kinematic ship actor really moving through one persistent world, with the server publishing its pose and characters standing on it through `CharacterMovementComponent` based movement.

Each run has a listen host and one or more clients as separate headless processes on one PC (UE 5.8.4, Development editor binaries, uncooked, `t.MaxFPS 60`). Each runs a scripted walker that tours bridge, corridor and engineering steps, jumps every 9 seconds, and uses the console. The server drives a 185-second profile: rest, accelerate to 10 m/s, turn both ways at 3 deg/s, pitch and roll to 10 degrees and back, a hard manoeuvre at 25 m/s and 8 deg/s, then brake.

Run it with `python Scripts/build/run_motion_matrix.py`, which takes about 25 minutes. Results go to the system temp folder.

## Current results

| Run | Players | Ship | Median ping | Corrections | Visible pops (largest) | Largest visible snap |
|---|---|---|---|---|---|---|
| `control_static` | 2 | Still | 16 ms | 0 | 0 | none |
| `moving_lan` | 2 | Moving | 15 ms | 1 | 0 | 35 cm |
| `moving_rtt100` | 2 | Moving | 134 ms | 6 | 0 | 4 cm |
| `moving_rtt200` | 2 | Moving | 238 ms | 43 | 3 (71 cm) | 80 cm, 5 over 10 cm |
| `crew_lag_worse` | 2 | Moving | about 240 ms | 33 | 2 (69 cm) | 77 cm, 3 over 10 cm |
| `moving_shipgravity` | 2 | Moving | 15 ms | 1 | 0 | 17 cm |
| `crew_4players` | 4 | Moving | 15 ms | 0 / 1 / 1 | 0 | none (first client only) |

Across these runs: 0 frames outside the hull, base never lost, no stuck waypoints. In the two-player runs without `crew_lag_worse`, ship attitude agreed within 0.1 degrees, and ship position to under 14 cm, or under 5 cm at normal ping.

`crew_4players` and `crew_lag_worse` include soft collision. The other rows are from just before it. Corrections at about 240 ms are not gone: the remaining ones are not explained by frame stalls, and were not explored further, as 3% packet loss is a harsh test.

How to read this:

- Each row is a single run. Clean runs have repeated well (0 and 1 corrections on separate runs), but small differences mean nothing.
- `rtt100` and `rtt200` are requested settings, with 10 ms / 1% and 20 ms / 3% jitter and loss. Use the measured ping.
- **Corrections** counts server corrections of any size. Most are not visible, because the client replays its moves afterward.
- **Visible pops** is a frame-to-frame jump in the ship's frame beyond what walking explains.
- **Visible snap** is position before a correction against position after replay. **It may overstate.** Several runs show a single 16 to 39 cm snap from a correction of a few centimetres, while the pop detector saw nothing. That disagreement is not explained, so treat these single values with suspicion.

## What was wrong, and what fixed it

### Telemetry writes to a slow disk were stalling the game

`H:` is a slow WD Green hard drive. Flushing telemetry to it about 10 times a second from each process caused hundreds of slow frames.

| Telemetry written to | Frames over 50 ms | Corrections | Pops | Worst frame |
|---|---|---|---|---|
| `H:` hard drive | 828 | 100 | 43 | 400 ms |
| `C:` SSD | 0 | 1 | 0 | 17 ms |

Results now default to the temp folder, and telemetry flushes about every 10 seconds. **Every result in the first version of this report came from a stalling harness, including `control_static`.** The game may also see some stalls from this drive in normal use, since the project lives on it. Moving the project to an SSD is worth considering.

### The client and server limits on a long frame did not match

A client caps a move at 125 ms, while `p.NetServerMaxMoveDeltaTimeScalar` lets the server accept 1.75 times that. Any frame over 125 ms made the server walk further than the client had, which forced a correction. `Config/DefaultEngine.ini` now sets the scalar to 1.

| Stalls present, one run each | Frames over 50 ms | Corrections | Pops |
|---|---|---|---|
| Default scalar (1.75) | 828 | 100 | 43 |
| Scalar 1 | 820 | 2 | 2 |

Real players hitch from loading and shader compiles, so this matters beyond the test. It has been measured with stalls only in that one comparison, before the hull change below. Now that the harness no longer stalls itself, `moving_stalls` does not exercise this any more (worst frame 39 ms), so a deliberate stall test is still needed. Possible downside: Epic chose 1.75 for some reason that has not been established, and a tighter limit could matter when moves are merged or delayed. Without stalls, runs at about 140 ms showed 1 correction with the default and 6 to 8 with scalar 1. That might be noise, but check it.

### Other players could appear 16 metres away

Under packet loss, another player would jump about 16.7 m for a moment, and back. Replicated position is relative to the movement base, and the hull was many separate components, so the base and position could arrive separately and be combined wrongly. All hull collision is now one component (`HullCollision`), so the base never changes, and the jump did not recur in the two lag runs afterward. **Any real hull mesh must keep this property.**

### The ship clock recovered slowly

After a stall, `GetServerWorldTimeSeconds` stays wrong for a long time, because it is a running average of about 250 updates (roughly 25 seconds). The client was about 0.85 s out for around 15 seconds. The vessel now takes its clock from its own snapshots, and snaps if it is more than 0.25 s out. This fix comes from reading the data and the engine code. It has not been measured against a real stall since, beyond ship disagreement falling from p95 68 cm to 16–18 cm on stalling runs.

### Earlier fix: rotation rates

The first runs had 309 corrections and the client's ship up to 161 degrees off. `FRotator` net serialization wraps each axis into [0, 360), so a rate of -2 deg/s arrived as +358 deg/s. Rates are now sent as an `FVector`.

### The original correction size was misleading

The first report's "largest correction" compared the current position with an acknowledgement one round trip old, so it grows with speed times ping even when nothing visibly jumps. The figure of about 2 m at high ping came from this and from the stalls, and it was not a real 2 m snap. Visible pops and visible snaps now measure what a player sees.

## Players colliding: soft collision

Hard collision between players was the main remaining source of corrections, because each client sees the others slightly in the past and so disagrees about who bumped whom. The owner chose soft collision.

Crew capsules no longer block each other. Instead, whoever controls a character adds a small movement input away from anyone within 90 cm, up to 35% of full input (`AThalassaCharacter::ApplyCrewNudge`). Input is already predicted and sent to the server, so this needs no new replication and should not cause disagreement. The values are placeholders, tuned by no one yet.

| Four players, moving ship, one run each | Corrections per client | Pops per client | Time any pair overlaps |
|---|---|---|---|
| Hard collision | 158 / 1430 / 140 | 70 / 1106 / 6 | not measured |
| Soft collision | 0 / 1 / 1 | 0 / 0 / 0 | 8.8% |
| Pass through, no nudge | 2 / 1 / 1 | 0 / 0 / 0 | 59.2% |

Two players spawned on the same spot, with nobody walking, ended about 90 cm apart on both a still ship and a moving one, with no corrections. Both were already apart when measurement began, so how long separating takes was not measured.

The 8.8% is partly the test: four walkers keep meeting in 2 m doorways, where there is little room to separate. How the nudge feels has not been tried by a person, and nobody has tested it with more than four players.

## Airlock and swimming

A server-run airlock off the corridor, and a scripted diver who repeatedly floods it, swims 15 m out, stops for 8 seconds while the ship carries on, catches the ship, returns through the outer door, drains and walks back. Run with `run_motion_test.py --eva` or `--eva-all`. These use a slow profile: up to 2 m/s, with gentle turns, 3 degrees of pitch and roll.

How it works:

- **Water is by compartment, in ship coordinates.** Everything outside a list of boxes is sea. The chamber is water up to its replicated flood level. There is no water volume or fluid simulation.
- **Swimming is `MOVE_Flying`**, chosen in `UpdateCharacterStateBeforeMovement`, which runs in every move on client, server and replay.
- **The base follows the location.** In the flooded chamber you stay on the hull, and past the outer door you leave it, keeping world-frame velocity across the change.
- **The interlock is on the server.** Both doors are never open together, and a door will not close on someone in the doorway.
- **A door stops being solid as soon as it starts opening**, because a client's door is always lower than the server's. Walking through one that is still rising clips visually.

| Run (one each) | Divers | Median ping | Complete trips | Stuck | Corrections | Pops (largest) | Largest visible snap |
|---|---|---|---|---|---|---|---|
| `door_eva` | 1 | 16 ms | 8 | 0 | 18 | 0 | 11 cm |
| `door_lag` | 1 | 134 ms | 8 | 0 | 114 | 28 (35 cm) | 39 cm |
| `door_lag_worse` | 1 | 238 ms | 8 | 0 | 211 | 33 (50 cm) | 62 cm |
| `door_four` | 4 | 15 ms | 7 each | 0 | 37 / 48 / 32 | 2 / 5 / 2 (34 cm) | 15 cm (first client only) |

Before doors stopped being solid, those first three runs had 55, 164 and 262 corrections. A walkers-only run afterward had 1 correction and no pops, so walking aboard did not regress. In `door_four`, the closest pair ending at 57 cm is expected: the divers finish crowded together.

**Open: corrections at high ping.** They cluster at the outer door and in the chamber, swimming on both sides, where a character changes between the ship's frame and the world's. The likely cause is that a client shows the ship slightly late, so it crosses that threshold at a slightly different place from the server. That has not been confirmed. Part of the correction count is the known metric inflation. Visible pops are the better guide.

**Placeholder:** no suit, oxygen, pressure, buoyancy, currents or water rendering. There is no surface, so a swimmer can rise forever. Above 2 m/s is untested, and a ship faster than the 4 m/s swim speed simply leaves a diver behind, with no rescue yet. Water surface in the chamber shows only from above.

## Not tested yet

The Phase 1 gate still needs:

- Approach B or C. The design document asks for a comparison, though approach A now gives little reason to look further unless something below fails.
- Packaged builds, two real PCs, or any real internet path. These runs all use loopback.
- A person using the airlock or swimming. So far that is only scripted divers, and a single rendered frame of the chamber with its doors at rest.
- Ladders, seated stations, loose objects.
- Eight players. Save and load in motion, joining mid-voyage.
- Any test with rendering on. The owner has tried it in the editor with two players and it felt good, but that is one informal session with almost no lag.

Ship-relative gravity looked good again on one run, but mouse look does not yet handle a tilted gravity direction.

## Next steps

1. The owner tries soft collision, the airlock and swimming in the editor.
2. A packaged build tested on two real PCs, the largest remaining unknown.
3. High-ping corrections at the outer door.
