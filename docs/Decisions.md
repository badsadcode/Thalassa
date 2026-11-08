# Decisions

Concrete decisions as prototypes settle them. Newest first. Anything here can be revisited when evidence changes.

## 2026-11-07

**The sonar chart is fixed to the world, in 25 m cells, and only measured cells exist.** It is replicated as one shared survey, and each client draws its own texture from it. See [SONAR.md](SONAR.md).

**The ship must not be a cigar with rooms in a row (owner's direction).** No `[BRIDGE][ROOM][AIRLOCK][ROOM]` chain. The hull has a varied silhouette, and each deck is a network: main corridors that loop, side corridors, junctions, and more than one route between places. Rooms vary in size and shape, and sit off corridors rather than opening into each other. Watertight doors are placed at points along corridors, not between every room. The current three-compartment graybox is only a test rig.

**The ship should be easy to get lost in at first (owner's direction).** Asymmetric layouts, so players need time to learn the ship before it feels like home. Stairs and ladders connect places as well as lifts. Floors within a deck are not all level: a few steps up or down between areas, and a bridge with stations on different levels, in the spirit of a starship bridge. Navigation help should stay light so that learning the ship is part of the game, and never block an emergency.

**Working ship direction: about 280 m, about 50 m wide, 6 decks, crew of 40 to 60 (owner's direction, not final).** Between a starship and a submarine, in the spirit of the owner's seaQuest and Enterprise references, without copying them. Not cramped, through wide main corridors, high ceilings and a few grand spaces rather than extra length. The R/V Horizon and R/V Nereis concept sheets are a guide for how to build it, and all of this will change.

**Setting: the ocean world Thalassa, settled by humanity for generations (owner's direction).** This answers the design document's open "alien planet or future Earth" question, and explains the cities, industries, trade networks, and people who find the ocean normal. An initial sketch, public at `site/src/pages/world.astro`, and expected to change.

**The Long Call and the Viator are never objectives (owner's direction).** Players find them organically, perhaps after dozens of hours of ordinary exploration. No quest marker, no "Find the Viator: 0/1". Consequences for systems:

- Rare acoustic events need a stable signature identity, so a clip recorded early can be recognised as the same sound many expeditions later. The hydrophone archive must keep these for the life of a save.
- Nothing may announce what a recording is. Recognising it is the player's job, with the archive's comparison tools at most suggesting a match.
- Nothing may steer the player toward them, and a whole game without ever solving them must still be complete.

**Tone: the crew can be funny, the setting is not (owner's direction).** Sea Chicken recipes and a sarcastic ship computer sit alongside exploration, science, isolation and danger, all taken seriously.

**Crew use soft collision (owner's choice).** Crew do not block each other, and are nudged apart through ordinary movement input. Hard collision caused up to 1430 corrections per client with four players; this caused at most 1. The nudge strength is untuned.

**Water is decided by ship compartment, and swimming uses `MOVE_Flying`.** It needs no water volume, moves with the ship, and is predicted like any other move. A real swimming mode can replace it behind the same rule.

**A door stops being solid as soon as it starts opening.** Clients always see a door lower than the server does. The cost is that someone can clip through a door that is still rising.

**All hull collision is one component.** A character's movement base must not change as they walk around the ship. Under packet loss, several bases made other players appear 16 m away. A real hull mesh must keep this.

**`p.NetServerMaxMoveDeltaTimeScalar=1`.** The default of 1.75 forces a correction on every frame over 125 ms. Revisit if corrections appear with merged or delayed moves.

**The vessel's client clock comes from its own snapshots**, not `GetServerWorldTimeSeconds`, which takes around 25 seconds to recover from a stall.

**Test results go to the system temp folder, and telemetry flushes rarely.** Frequent writes to a slow disk stalled the game and dominated the results.

## 2026-11-06

**Engine: Unreal Engine 5.8.** The installed version. 5.8.4 at the time of writing.

**Keep testing approach A (really moving ship) for now.** Not a final choice. No results so far rule it out, and approaches B and C have not been built. See [MOVING_SHIP_SPIKE.md](MOVING_SHIP_SPIKE.md).

**The vessel replicates its own pose snapshot and does not use `ReplicateMovement`.** That gives one clear source of truth, and lets clients dead-reckon and smooth the ship themselves.

**Never replicate a signed rate in an `FRotator`.** Its net serialization wraps each axis into [0, 360).

**Hull pieces are default subobjects.** They need stable network names to be referenced as a movement base. Temporary graybox: a real hull mesh replaces this.

**Spike controls and the expanded station view are not final.** Input objects are created in code rather than as assets, and the station view is drawn on the HUD canvas rather than in UMG. They exist to prove shared state, and should be replaced when that work starts.

**Devlog: Astro, static, self-hosted fonts, no analytics.** See [site/README.md](../site/README.md).

**Status labels use the brief's section 6 wording** (`concept`, `prototype`, `in-development`, `implemented`, `tested`) instead of the example frontmatter in section 4, because section 6 is the rule about honest labelling.

**Stop polishing the grey deck (owner's direction).** The models and graphics will be replaced. What is worth keeping is in [INTERIOR_RULES.md](INTERIOR_RULES.md).

## Planned, not started

**Internet play (owner's direction).** Friends must not have to install anything to join. The owner is behind CGNAT, with no public address, so hosting needs NAT traversal or a relay.

- **Now, for testing:** the host runs a UDP tunnel such as playit.gg to port 7777, and friends type its address and port into Join.bat. Not yet tried.
- **For the real game:** Epic Online Services peer-to-peer, with its free relays as fallback. This needs the owner's free Epic developer account and a product in the Developer Portal. Whether it works from behind CGNAT must be proven by someone outside the network joining.

**Helm (owner's direction).** A first version is built, following this: see `HELM.md`. The old test keys that steered from anywhere are gone.

- Steering happens from the helm station, by whoever is sitting there, and depends on ship systems working at least somewhat.
- Handling is somewhat realistic without being a submarine simulator. You feel the ship's weight and its power, and upgrades can make it feel more powerful.
- Two control modes:
  1. **Precise:** hold a button to move, turn and manoeuvre.
  2. **Set and go:** push the throttle to a setting and the ship keeps going, with the same idea possibly for steering.
- The controls exist physically at the helm too: a throttle lever that visibly moves, plus a monitor and a heads-up display showing throttle and the other stats. Keys, lever, monitor and HUD all work from the same ship state, so crew nearby can see what the helm is doing.
- **Full stop (owner's request, built):** one button, or a lever, that stops all the engines and stops the ship in place. Built as **X** at the helm and a red button on the helm console that anyone can press.

**Player movement (owner's direction).** Walking the ship takes too long, so there is sprinting (**Shift**, built). In the final game, movement should be smooth and fluid, like Titanfall: fast, keeping momentum, with moves like a slide. It should still work on a moving ship over the network, so every new move needs the same testing as walking did.

- **Built so far:** sprinting, crouching, a slide (C while sprinting) that keeps your speed and can be jumped out of, and a view that eases down and up instead of snapping when you crouch.
- **Possible next moves:** a longer or boosted slide, mantling onto ledges, and wall running if the ship's spaces suit it.

**Stations and their monitors (owner's direction).** Following design document sections 5.6 and 17: each station has physical screens in the world, and a key expands the picture to read it properly.

- **A camera station:** cameras mounted on the hull, showing what is to port, to starboard and around the ship. The station shows them all together and one at a time on a bigger screen, and may later have several monitors. This fits section 6.1: an external camera helps, as long as the player stays a crew member rather than a detached overhead camera.
- **Other stations** will have monitors for the ship's systems as those systems arrive.

**A holotable aboard (owner's idea, 2026-11-08).** A table with a 3D hologram of the terrain below and around the ship, a little simplified, moving as the ship moves: a place where the crew gather and decide what to do next. First version in the mess hall. See [HOLOTABLE.md](HOLOTABLE.md).

**The seabed follows the owner's environment reference (2026-11-08).** Varied at every scale, shaped by geology, mostly natural, sometimes alien, often deliberately empty. The reference is private (`docs/THALASSA_Seabed_Environment_Reference.md`, not in git). How it is built, and Claude's notes, are in [SEABED.md](SEABED.md).

**Biomes, life and bioluminescence (owner's direction, 2026-11-08).** The ocean has zones with their own life and mood: dark and gloomy ones, and a little brighter ones with fluorescent plants and animals, with deep-sea vegetation and creatures. The goal is the feeling of thalassophobia. Generated shapes (Blender rock and tree generators) with scanned CC0 textures. See [SEABED.md](SEABED.md).

**Crew characters: MetaHuman, later (2026-11-08).** The owner asked about MetaHuman for the player characters. It is free for Unreal Engine projects, and its creator is built into the editor from UE 5.6. It needs the owner's Epic account. To plan for: the cost of up to 8 characters (lower detail levels and simpler hair at distance), diving suits and helmets over the body, and an animation setup on top of the existing movement and replication. Scheduled after the world and the ship's interior.

**The ship is modelled last (owner's direction, 2026-11-08).** The final submarine has several decks, an EVA hangar (small 1–3 crew subs, ROVs, the work mech) and every station. The owner will model it once the game's systems and stations exist, so the hull wraps a finished layout instead of being redone. Until then, the grey deck and the walkable shell carry gameplay. The agreed workflow, for when it starts: inside out. Shell blockout first, then interior kit pieces as Packed Level Actors, then the hull modelled around it in ship coordinates with a gap of half a metre or more, and windows cut through hull and interior wall with a shell box over the glass. Players inside and divers outside see each other through windows with no special work, because the ship is one actor in one world. Don't push ship modelling before then.

## Still open

From design document section 26: ship size, companion name, and the rest of that table. Source-code licence is the owner's choice and has not been made.
