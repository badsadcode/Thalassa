# Rules for the ship's interior

The grey deck is temporary, and the final interiors will replace it. These rules are not temporary: each one came from a test that failed. A final model that breaks one brings its bug back.

## Collision

1. **All walkable collision on the ship is one component.** A character's position is sent relative to what they stand on. With several separate pieces under packet loss, other players appeared 16 m away. Detail meshes can be separate, but they must not be walkable or be a movement base.
2. **Stairs are walked as a hidden ramp.** The steps you see must not be collision. On real steps, walking downstairs becomes a run of tiny falls, which looks like stumbling and causes corrections. Keep ramps under about 44 degrees, the walkable limit. The current stairs are 32 degrees.
3. **Steps between floor levels stay under 45 cm.** Characters step up 45 cm on their own. Taller changes need stairs or a ramp.
4. **Walls must reach every floor they enclose.** A room with a lower floor, like engineering, needs walls down to that floor, or there is a gap out to the sea.
5. **Doorways are at least 2 m wide and 2.5 m high** on main routes, so the crew nudge can pass two people.

## Water and leaving the ship

6. **Every space anyone can stand in is inside a dry compartment box.** Outside them is sea, and you start swimming. Getting this wrong means swimming indoors or walking outside.
7. **A door stops being solid as soon as it starts opening.** Clients always see a door lower than the server does. A door must never be solid while it is moving up.
8. **Airlock doors check the doorway is clear before they close.** Leave at least 55 cm between where people stand and the door's plane.

## Ladders

9. **Every ladder has a climbing zone** in front of it: about 25 cm deep, its full width, and from its foot to just above its top. The visible rungs are not collision.
10. **Known problem, cause not found:** when two walkers reach a ladder foot together, neither manages to climb. They hover at the edge of the climbing zone, switching between climbing and falling many times. Switching the crew nudge off within 1.5 m of a ladder did not help, so the nudge is not the cause. A single walker climbs fine. Start by logging movement mode and position each frame for both characters at the ladder foot.

## Lights

11. **Sealed spaces need their own lights.** Nothing from outside lights a closed hull, and the first deck was completely black. Lights must move with the ship.

## Checking a new interior

- Capture frames: `python Scripts/build/capture_screenshots.py <folder> name=yaw@x,y,z`, in ship coordinates.
- Let walkers tour it: `python Scripts/build/run_motion_test.py --deck`. Stuck waypoints mean somewhere is unreachable, and the log names where. Walkers bunch together over time, which makes the numbers worse than one person would see.
