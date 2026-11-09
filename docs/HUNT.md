# Phase one: the leviathan hunt

The owner's direction (2026-11-09): the first playable goal of THALASSA is a leviathan hunt. The crew take the ship out, find the leviathan, track it, and fight it, while it fights back: the ship gets damaged, floods, and has to be kept running. This file is the plan and its state.

## The steps

Each is playable before the next starts.

1. **Damage and repairs.** Ship systems that can be damaged, hull breaches that flood compartments, repairs done by the crew together. Damage from collisions first, then from the leviathan. *Started 2026-11-09: see below.*
2. **A leviathan that hunts.** Today it's worked out from the clock on every machine and only curious. A hunter has to react to the ship, so the host will run its behaviour and replicate it: roam, investigate, stalk, attack (ram, tail strike, coil round the hull), flee when badly hurt. It gets health and weak points (eyes, glowing spots). The biggest change.
3. **Weapons.** Harpoons on a tether, to track it and wear it down; torpedoes, few, powerful and loud; maybe decoys or flares to lure it away.
4. **Tracking.** Sonar contacts and the hydrophone (hear its calls before you see it), and signs it leaves behind.
5. **The loop.** Set out, find its trail, track it, fight. Win by killing it or driving it off; lose if the ship is crippled.

## Step 1: damage and repairs (first version)

- **Ship systems,** each with health from 0 to 100%, replicated:
  - engines: thrust scales with health, down to a limp at 0;
  - steering: turning and the planes scale with it;
  - sonar: below 30% the survey and the ping are out;
  - lights: the hull floodlights dim and flicker as it falls, and go out at 0;
  - pumps: how fast flooded compartments drain.
  Each has a place aboard where it is repaired: engines and steering gear in the engineering hall, pumps aft, the light panel midships, the sonar at its console on the bridge.
- **Breaches.** A hit tears a hole in the compartment nearest it, at the hull, with a severity from 0 to 1. Water pours in, faster the worse the breach, and the pumps fight it. A compartment's water level is replicated; below it, players swim. Compartments are treated as watertight from each other for now.
- **Repairs.** E near a damaged system or a breach starts repairing it; you keep repairing while you stay within reach. Several people on one thing repair it faster.
- **Damage sources:** hitting the seabed or anything solid, harder the faster you hit. In test builds, **K** knocks the ship somewhere at random; `-ThalassaTestDamage=<seconds>` does it on a timer.
- **What you notice:** the impact's thud, the interior lights turning red while the hull is breached, the floodlights flickering, water rising, and a damage panel on the HUD with what's broken, what's flooding, and what you can repair from where you stand.
- **Code:** `UThalassaShipDamageComponent` (`Source/Thalassa/Vessel/ThalassaShipDamage.*`), on the vessel. The ship asks it for its engines' and steering's factors (`TickHelmMotion`), the sonar whether it works (`TryPing`, the survey sweep), and each compartment's water level (`IsWaterAt`). Collisions call `ApplyImpact` with a severity from how fast the hull was going, at the point of the hull furthest along that way. The breach spray is `M_BreachSpray` (`create_damage_materials.py`).
- **Numbers:** a breach of severity 1 fills its compartment in 50 s; working pumps drain 0.8% of every flooded compartment a second; a person repairs a system 7% a second and closes a breach 5% a second; hits below 0.2 dent without breaching; systems within 40 m of a hit lose up to 70% (the nearer the more), and every hit costs the lights a quarter of its severity. Repair spots are on the main deck (`SystemSpot`); breaches sit at the compartment's wall nearest the hit, about chest height.
- **Water and the deck:** a compartment's box starts under its floor plates, so the first quarter or so of flooding is under the deck, like bilge water, before it shows and players wade, then swim.
- **Switches:** **K** (test builds) knocks the ship; `-ThalassaTestDamage=<seconds>` does it on a timer (`run_motion_test.py --damage-every`); `-ThalassaAutoRepair` makes every player repair whatever is in reach, as if they pressed E; `-ThalassaDamageDebug` logs where the first breach's spray and the fullest compartment's water are drawn. Every 5 s of the shared clock each machine logs `DAMAGESTATE,<time>,<fingerprint>,<breaches>,<hits>`.

### Tested (2026-11-09)

- **Two players on the deck, a knock every 12 s** (the scripted ship motion, walkers aboard, about 19 ms): the damage state matched between host and client in 36 of 38 five-second samples (the other two a moment apart), with the same breaches and hits in all 38. 4 corrections (largest 6.7 cm). By the end, 19 knocks had left 15 breaches and the pumps at 9%: the ship was flooding, and 2 to 3 walker waypoints got stuck where the scripted walkers had to swim.
- **Repairs:** a player at the light panel with `-ThalassaAutoRepair` started on the lights after each knock and had them whole again in about 3 s from 79%.
- **Frames:** red pulsing interior light, the damage panel, rising water in the engine room, a breach's glowing spray on the midships wall.
- **Not tested yet:** damage from a real collision at the helm (the code runs on every collision, but no test drove into rock), and repairing a breach by hand.
