"""Compares telemetry from a moving-ship test run. Standard library only.

    python Scripts/build/analyze_motion_test.py Saved/MotionTests/baseline
"""
import bisect
import csv
import statistics
import sys
from pathlib import Path

RUNNING = 1  # EVesselProfileState::Running
AUTONOMOUS_PROXY = 2  # ENetRole::ROLE_AutonomousProxy
CAPSULE_DIAMETER_CM = 68
MOVE_FLYING = 5  # EMovementMode::MOVE_Flying, used for swimming.

# Must match ThalassaVesselLayout::Profile in ThalassaVessel.cpp.
SEGMENTS = [
    ("rest", 15), ("accelerate", 15), ("cruise", 15), ("turn_stbd", 20), ("turn_port", 20),
    ("pitch_up", 15), ("level_pitch", 15), ("roll", 15), ("level_roll", 15),
    ("hard_manoeuvre", 20), ("brake", 20),
]


def load(path):
    with open(path, newline="") as handle:
        rows = []
        for row in csv.DictReader(handle):
            try:
                rows.append({k: float(v) for k, v in row.items()})
            except (TypeError, ValueError):
                continue  # A process stopped mid-line.
        return rows


def percentile(values, fraction):
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(len(ordered) * fraction))]


def interpolate(series, times, t, keys):
    """Linear interpolation of a time-sorted series; None outside its range."""
    i = bisect.bisect_left(times, t)
    if i == 0 or i >= len(times):
        return None
    a, b = series[i - 1], series[i]
    span = b["t"] - a["t"]
    if span <= 0 or span > 1.0:  # Do not interpolate across a telemetry gap.
        return None
    f = (t - a["t"]) / span
    return [a[k] + (b[k] - a[k]) * f for k in keys]


def distances(series_a, series_b, keys):
    times_b = [r["t"] for r in series_b]
    result = []
    for row in series_a:
        other = interpolate(series_b, times_b, row["t"], keys)
        if other is not None:
            result.append(sum((row[k] - v) ** 2 for k, v in zip(keys, other)) ** 0.5)
    return result


def angle_distances(series_a, series_b):
    """Largest per-axis attitude difference at matching times. Angles wrap, so interpolate by delta."""
    times_b = [r["t"] for r in series_b]
    result = []
    for row in series_a:
        i = bisect.bisect_left(times_b, row["t"])
        if i == 0 or i >= len(times_b):
            continue
        a, b = series_b[i - 1], series_b[i]
        span = b["t"] - a["t"]
        if span <= 0 or span > 1.0:
            continue
        f = (row["t"] - a["t"]) / span
        worst = 0.0
        for key in ("ship_yaw", "ship_pitch", "ship_roll"):
            step = (b[key] - a[key] + 180.0) % 360.0 - 180.0
            worst = max(worst, abs((row[key] - (a[key] + step * f) + 180.0) % 360.0 - 180.0))
        result.append(worst)
    return result


def separation(rows):
    """Closest pair of crew in the ship's horizontal plane, per telemetry sample."""
    by_time = {}
    for row in rows:
        by_time.setdefault(row["t"], []).append(row)
    closest = []
    for t in sorted(by_time):
        crew = by_time[t]
        gaps = [((a["lx"] - b["lx"]) ** 2 + (a["ly"] - b["ly"]) ** 2) ** 0.5
                for i, a in enumerate(crew) for b in crew[i + 1:]]
        if gaps:
            closest.append(min(gaps))
    return closest


def summary(label, values, unit="cm"):
    if not values:
        return f"  {label}: no overlapping samples"
    return (f"  {label}: median {statistics.median(values):.1f} {unit}, "
            f"p95 {percentile(values, 0.95):.1f} {unit}, max {max(values):.1f} {unit}")


def report(folder):
    folder = Path(folder)
    runs = {p.stem: load(p) for p in sorted(folder.glob("*.csv"))}
    lines = [f"=== {folder.name} ==="]
    if "host" not in runs or not runs["host"]:
        return "\n".join(lines + ["  FAILED: host produced no telemetry"])

    host_running = [r for r in runs["host"] if r["profile"] == RUNNING]
    if not host_running:
        return "\n".join(lines + ["  FAILED: motion test never started on the host"])
    start = host_running[0]["t"]
    end = host_running[-1]["t"]
    players = {r["player"] for r in host_running}
    lines.append(f"  host saw {len(players)} player(s), motion test ran {end - start:.0f} s")

    closest = separation(host_running)
    if closest:
        overlapping = sum(1 for gap in closest if gap < CAPSULE_DIAMETER_CM) / len(closest) * 100
        tail = closest[-len(closest) // 10:]
        lines.append(f"  crew spacing: overlapping {overlapping:.1f}% of the time, "
                     f"closest pair at the end {statistics.median(tail):.0f} cm")

    client_players = {r["player"] for name, rows in runs.items() if name != "host"
                      for r in rows if r["local_role"] == AUTONOMOUS_PROXY}

    for name, rows in runs.items():
        moving = [r for r in rows if r["profile"] == RUNNING]
        if name == "host":
            # Every pawn has authority on the host, so its own pawn is the one no client owns.
            own = [r for r in moving if r["player"] not in client_players]
        else:
            own = [r for r in moving if r["local_role"] == AUTONOMOUS_PROXY]
        if not own:
            lines.append(f"  {name}: NO SAMPLES of its own pawn during the motion test")
            continue
        last = own[-1]
        pings = [r["ping"] for r in own]
        ungrounded = sum(1 for r in own if r["based"] == 0) / len(own) * 100
        lines.append(
            f"  {name}: {last['corr']:.0f} corrections (largest {last['corr_max']:.1f} cm), "
            f"{last['pops']:.0f} ship-frame pops (largest {last['pop_max']:.1f} cm), "
            f"{last['airborne']:.0f} unplanned airborne, {last['stuck']:.0f} stuck waypoints, "
            f"{last['outside']:.0f} frames outside hull, no base {ungrounded:.1f}% of samples, "
            f"worst frame {last['worst_frame'] * 1000:.0f} ms, ping median {statistics.median(pings):.0f} ms")
        lines.append(f"    beeps seen at end: {moving[-1]['beeps']:.0f}")
        swimming = sum(1 for r in own if r["mode"] == MOVE_FLYING) / len(own) * 100
        if last.get("eva") or swimming:
            lines.append(f"    EVA: {last.get('eva', 0):.0f} complete trips, swimming {swimming:.0f}% of samples")

    for name, rows in runs.items():
        if name == "host":
            continue
        pose_keys = ["ship_x", "ship_y", "ship_z"]
        first_row_per_t = {}
        for row in rows:
            if row["profile"] == RUNNING:
                first_row_per_t.setdefault(row["t"], row)
        client_ship = sorted(first_row_per_t.values(), key=lambda r: r["t"])
        host_ship = {}
        for row in host_running:
            host_ship.setdefault(row["t"], row)
        host_ship = sorted(host_ship.values(), key=lambda r: r["t"])
        lines.append(f"  {name} vs host:")
        lines.append(summary("ship position disagreement", distances(client_ship, host_ship, pose_keys)))
        lines.append(summary("ship attitude disagreement", angle_distances(client_ship, host_ship), "deg"))
        for player in sorted(players):
            a = sorted((r for r in rows if r["player"] == player and r["profile"] == RUNNING), key=lambda r: r["t"])
            b = sorted((r for r in host_running if r["player"] == player), key=lambda r: r["t"])
            who = "own pawn" if a and a[0]["local_role"] == AUTONOMOUS_PROXY else "other player's pawn"
            lines.append(summary(f"player {player:.0f} ({who}) ship-frame disagreement", distances(a, b, ["lx", "ly", "lz"])))

        own = [r for r in rows if r["local_role"] == AUTONOMOUS_PROXY and r["profile"] == RUNNING]
        if own and end - start < 200:  # The labels only fit the standard profile.
            lines.append("  corrections by manoeuvre:")
            segment_start = start
            cells = []
            for label, duration in SEGMENTS:
                in_segment = [r for r in own if segment_start <= r["t"] < segment_start + duration]
                count = in_segment[-1]["corr"] - in_segment[0]["corr"] if in_segment else 0
                cells.append(f"{label} {count:.0f}")
                segment_start += duration
            lines.append("    " + ", ".join(cells))
    return "\n".join(lines)


if __name__ == "__main__":
    print(report(sys.argv[1]))
