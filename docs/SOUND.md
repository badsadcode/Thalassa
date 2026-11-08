# Sound

The design document's Phase 2 asks for a machine idle loop, an acoustic environment, console bleeps and a distinct hull impact thud. All four exist now. Like the sonar ping, every sound is **synthesised as it plays**, in code, with no sound assets: `Source/Thalassa/Vessel/ThalassaShipAudio.*`. They're placeholders in the spirit of the grey deck, built so the timing and the feel can be judged now. Recorded or designed sounds can replace any of them later behind the same events.

Each machine plays its own sounds from state that's already replicated, so **nothing about sound goes over the network**. The vessel's `TickSound` sets each sound's parameters and notices events as changes in replicated state. The first look only remembers, so joining a game in progress is silent.

## What plays

| Sound | Where | What it is |
|---|---|---|
| Machinery (`MachinerySound`) | The middle of the engineering hall, spatial: full within 15 m, gone by 85 m | A deep hum (46 Hz rising to 56 Hz with load) with a slow beat, a pump thumping on its cycle (1.6 s at idle, 1 s at full load), a whine rising with the throttle, and the soft hiss of air handling. Load follows the throttle lever or the ship's speed, and takes seconds to spin up or down. |
| Hull ambience (`HullAmbience`) | Each player hears their own, all around them | **Inside:** a faint ship-wide 47 Hz hum and a low rumble through the hull. **Water rushing past:** brighter and louder with speed, much louder out in the water. **Outside:** the sea's low rumble, never quiet. **Creaks:** steel groaning under pressure, a resonance sliding between two pitches, excited by uneven stick-slip clicks. They're rare near the surface (every ~28 s on average) and frequent in the deep (every ~7 s at 600 m), more often still while the ship turns. |
| Console bleeps (`HelmBleep`, `CameraBleep`, `SonarBleep`) | On each console's screen, spatial: heard within about 20 m | Rising two tones when someone takes the station or switches it on, falling when they leave or switch off. A click when the helm changes mode. A triple alarm when full stop is pressed. |
| Hull impact (`ImpactSound`) | Everyone aboard, all around them | A heavy thud (its pitch dropping from 85 to 30 Hz), a crash of noise, the steel ringing at 173, 281, 437, 611 and 893 Hz for up to 3 s, and a slow groan as the hull settles. Plays whenever the replicated collision count goes up. |

The ping (`SonarPing`, see `SONAR.md`) was already there.

## Checking sounds without hearing them

`-ThalassaRecordAudio=<seconds> [-ThalassaRecordAudioName=<name>]` records a game's own sound output to `Saved/BouncedWavFiles/<name>.wav`, starting 5 s in. `run_motion_test.py --record-audio <seconds>` does it for the host of a test.

Two traps, both found the hard way:
- **Unreal mutes a game without focus,** and a headless test never has focus. The recording comes out silent unless `-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1.0` is passed. `run_motion_test.py` passes it.
- **Without rendering (`-nullrhi`) the listener never moves.** Unreal updates the listener while drawing the frame, so in a headless run it stays at the world's origin, kilometres from the ship. Non-spatial sounds (hull ambience, impact) record fine. Spatial ones (machinery, bleeps, ping) are silent. To record those, run with a window (`UnrealEditor.exe -game -windowed`).

## Tested (2026-11-08)

- **The dive test, recorded on the host walking the deck** (`run_motion_test.py --helm --helm-course dive --ship-at=0,5000,-47000 --record-audio 100`). The ambience was steady at about −37 dBFS, with a 47 Hz hum. A creak came at 48 s. The bow hit the seabed 70.5 s into the recording, and the loudest moment, about 14 dB above the background, came at 70 s. Its spectrum had the thud around 55 Hz and the ring at 173, 281 and 437 Hz.
- **Standing in the engine room, with rendering on.** −29 dBFS, about 8 dB above the deck, with the machinery's harmonic series at 46, 92 and 138 Hz over the hull hum. Without rendering, the same spot recorded none of the machinery, which is how the listener trap above was found.
- **Not recorded:** the console bleeps, though they use the same spatial mechanism as the machinery; creaks at their full rate in the deep; and how anything sounds to a diver outside. Nobody has listened in the editor yet.

## Not done yet

- **Real sound design.** These are synthesised placeholders. Recorded machinery, real hull creaks and designed bleeps can replace them behind the same events.
- **Where the impact is heard.** The design document wants an impact near engineering to ring sharply there and arrive as a dull thump on the bridge. Today it's the same everywhere.
- **Occlusion between compartments,** and muffling through bulkheads.
- **Hydrophones and creatures:** listening outward, which is next in Phase 2.
- **Ship AI alerts:** the Standard and Attitude voices.
