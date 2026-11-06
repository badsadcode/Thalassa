# Decisions

Concrete decisions as prototypes settle them. Newest first. Anything here can be revisited when evidence changes.

## 2026-11-07

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

Collision between players: see the options in [MOVING_SHIP_SPIKE.md](MOVING_SHIP_SPIKE.md).

From design document section 26: setting, ship size, companion name, and the rest of that table. Source-code licence is the owner's choice and has not been made.
