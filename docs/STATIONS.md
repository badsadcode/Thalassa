# Stations

A station is a spot aboard that one person can take with **E** and leave with **E**. Whoever is there works the station, and nobody else can. Anyone who walks or is pushed more than 3 m away, or disconnects, leaves it. While you're at a station your character stays on its spot, and your movement keys work the station instead.

The code is `UThalassaStationComponent`. The helm (`HELM.md`) and the camera station are both stations. Future system stations should be too, so they all behave the same.

This follows design document sections 5.6 and 17: every station has physical screens in the world that anyone nearby can see, and the person at it gets the same picture on the HUD, with a key to expand it. The screens share `UThalassaConsoleScreenComponent`: each machine draws its own picture into a render target from replicated state, and only while the screen is on someone's screen.

## The camera station

It's the starboard console beside the sonar, on the bridge. Six cameras sit just outside the hull, one looking each way: bow, starboard, port, stern, keel (straight down) and dorsal (straight up). They're placed from the hull's bounds, so they follow the deck plan when it changes.

- **The large screen** shows the selected camera.
- **The smaller screen** shows all six at once, with the selected one outlined in amber.

At the station:

| Keys | At the camera station |
|---|---|
| A / D | Previous / next camera |
| Tab | The selected camera full screen |
| E | Stand up |

Without Tab, the HUD shows all six in a panel.

Which camera is selected is replicated, so everyone sees the operator's choice on the large screen. The pictures aren't replicated: every machine renders its own from its own copy of the world. Cameras only render while some screen or HUD on that machine is showing them. Then the selected camera updates 15 times a second, and the others take turns, 12 pictures a second between them. At 480 by 270 pixels, the grey ship costs very little, but real art will cost more, so the rates are constants in `ThalassaCameraStation.cpp`.

## Tested

- Frames from the bridge show both screens working. The bow camera looks ahead at the pillars on the seabed, the keel camera looks down on them, and the dorsal camera is black, because there is nothing above but empty sky.
- `run_motion_test.py --helm --helm-course cameras` sends a scripted operator on the first client to the camera station. He steps through every camera and back, then stands up. His log lines start `CAMERABOT`, and show the selection as the server sent it back.

## Not done yet

- **More monitors** at the camera station, and monitors at other stations as the ship's systems arrive.
- **Light outside.** The cameras see what the sun and fog show. Floodlights on the hull would help, especially deeper down.
- **Choosing a camera directly,** with number keys or by clicking the grid.
- **A seat.** As at the helm, you stand.
