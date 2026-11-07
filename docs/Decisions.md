# Decisions

Concrete decisions as prototypes settle them. Newest first. Anything here can be revisited when evidence changes.

## 2026-11-07

**Setting: the ocean world Thalassa, settled by humanity for generations (owner's direction).** This answers the design document's open "alien planet or future Earth" question, and explains the cities, industries, trade networks, and people who find the ocean normal. An initial sketch, public at `site/src/pages/world.astro`, and expected to change.

**The Long Call and the Viator are never objectives (owner's direction).** Players find them organically, perhaps after dozens of hours of ordinary exploration. No quest marker, no "Find the Viator: 0/1". Consequences for systems:

- Rare acoustic events need a stable signature identity, so a clip recorded early can be recognised as the same sound many expeditions later. The hydrophone archive must keep these for the life of a save.
- Nothing may announce what a recording is. Recognising it is the player's job, with the archive's comparison tools at most suggesting a match.
- Nothing may steer the player toward them, and a whole game without ever solving them must still be complete.

**Tone: the crew can be funny, the setting is not (owner's direction).** Sea Chicken recipes and a sarcastic ship computer sit alongside exploration, science, isolation and danger, all taken seriously.

**Crew use soft collision (owner's choice).** Crew do not block each other, and are nudged apart through ordinary movement input. Hard collision caused up to 1430 corrections per client with four players; this caused at most 1. The nudge strength is untuned.

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

## Still open

From design document section 26: ship size, companion name, and the rest of that table. Source-code licence is the owner's choice and has not been made.
