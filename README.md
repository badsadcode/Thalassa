# THALASSA

*Beep. Bloop.*

A first-person, 1 to 8 player co-op submarine exploration game in development at Bad Ideas Works, built with Unreal Engine 5.

**Status: early prototype.** There is no playable game yet. The current work is a technical test of the hardest problem in the design: a crew walking around inside a moving submarine over a network. See [docs/MOVING_SHIP_SPIKE.md](docs/MOVING_SHIP_SPIKE.md) for what has been measured so far.

## Requirements

- Windows 10 or 11
- Unreal Engine **5.8** (developed against 5.8.4)
- Visual Studio 2022 with the "Game development with C++" workload, MSVC 14.44 or newer (developed against 14.44.35229)
- Windows SDK 10.0.22621
- [Git LFS](https://git-lfs.com): run `git lfs install` before cloning
- Python 3 for the test scripts (standard library only)

No paid tools or plugins are required. See [docs/TOOLCHAIN.md](docs/TOOLCHAIN.md).

## Build

If your engine is not in `D:\EPIC\UE_5.8`, pass its location with `-EngineRoot`:

```bash
powershell -NoProfile -File Scripts/build/build-editor.ps1 -EngineRoot "D:\EPIC\UE_5.8"
```

Or right-click `Thalassa.uproject`, choose **Generate Visual Studio project files**, and build from Visual Studio.

The test map `Content/TestMaps/L_ShipMotion` is committed. To regenerate it, see `Scripts/unreal/create_test_map.py`.

## Try it

Open `Thalassa.uproject` in the editor and press Play. You start on the main deck, in the corridor north of the mess hall. Set **Number of Players** to 2 and **Net Mode** to **Play As Listen Server** to walk around with a second player.

| Keys | Action |
|---|---|
| WASD, mouse, Space | Walk, look, jump (in water: swim where you look) |
| Shift | Sprint |
| C | On foot: crouch. While sprinting: slide. Space jumps out of a crouch or slide, keeping its speed |
| Space / C | In water: up / down |
| E | Take or leave a station: the helm (middle of the console at the bridge's forward window) or the cameras (starboard console beside the sonar). Also presses the red full stop button, and cycles the airlock (off the corridor, to starboard) |
| At the helm | W/S throttle, A/D rudder, Space/C planes, R precise or set-and-go, X full stop. See `docs/HELM.md` |
| At the cameras | A/D choose a camera, Tab full screen. See `docs/STATIONS.md` |
| Tab | Toggle the expanded station view |
| N | Sonar on or off; Tab shows its chart |
| P | Run the scripted motion test |

## Test

The moving-ship test runs a real listen host and a real client as separate headless processes, with scripted walkers on board, while the ship runs through a fixed set of manoeuvres. It takes about four minutes.

```bash
python Scripts/build/run_motion_test.py --name moving_lan
```

Add `--rtt 100 --jitter 10 --loss 1` to simulate a bad connection, or `--clients 3` to add players. Results are written to a folder in your system temp directory, and the script prints where. `Scripts/build/run_motion_matrix.py` runs the standard set.

## Layout

| Path | Contents |
|---|---|
| `Source/Thalassa/` | Game code: `Vessel/`, `Characters/`, `Core/`, `Tools/` |
| `Scripts/` | Build and test scripts, editor automation |
| `Content/` | Unreal assets, stored in Git LFS |
| `docs/` | Decisions, toolchain, licences, test results |
| `site/` | The public devlog. See [site/README.md](site/README.md) |

## Licence

No licence has been chosen yet, so the default applies: all rights reserved by the author.
