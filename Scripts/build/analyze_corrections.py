"""Breaks a test run's client corrections down by what was happening at the time.

    python Scripts/build/analyze_corrections.py Saved/MotionTests/name [more folders...]
"""
import sys
from pathlib import Path

FALLING = 3
NEAR_PAWN_CM = 150
STALL_SECONDS = 0.1
RUNNING = 1


def classify(c):
    """Most specific explanation first; a correction is counted under one cause only."""
    if c["worst_frame"] >= STALL_SECONDS:
        return "frame stall (client)"
    if 0 <= c["near"] < NEAR_PAWN_CM:
        return "near other pawn"
    if c["client_mode"] == FALLING or c["server_mode"] == FALLING:
        return "airborne"
    if c["client_base"] != c["server_base"]:
        return "base differs"
    return "unexplained"


def read(path):
    rows = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        at = line.find("CORR,")
        if at < 0:
            continue
        p = line[at:].split(",")
        if len(p) != 13 or int(p[12]) != RUNNING:
            continue
        rows.append({"t": float(p[1]), "cm": float(p[2]), "client_mode": int(p[3]), "server_mode": int(p[4]),
                     "x": float(p[5]), "near": float(p[8]), "worst_frame": float(p[9]),
                     "client_base": p[10], "server_base": p[11]})
    return rows


def visible_snaps(path):
    """How far corrections really moved the player, after replaying unacknowledged moves."""
    values = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        at = line.find("SNAP,")
        if at >= 0:
            p = line[at:].split(",")
            if len(p) == 4 and int(p[3]) == RUNNING:
                values.append(float(p[2]))
    return sorted(values)


def stalls(path):
    """Slow frames during the test: count over 50 ms, count over 100 ms, and the worst frame in seconds."""
    frames = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        at = line.find("STALL,")
        if at >= 0:
            p = line[at:].split(",")
            if len(p) == 3:
                frames.append(float(p[2]))
    return len(frames), sum(1 for f in frames if f >= STALL_SECONDS), max(frames, default=0.0)


def report(folder):
    log = Path(folder) / "client0.log"
    rows = read(log)
    lines = [f"=== {Path(folder).name}: {len(rows)} corrections, {sum(1 for r in rows if r['cm'] < 10)} under 10 cm"]
    snaps = visible_snaps(log)
    if snaps:
        lines.append(f"  visible snap: median {snaps[len(snaps) // 2]:.1f} cm, p95 {snaps[int(len(snaps) * 0.95)]:.1f} cm, "
                     f"max {snaps[-1]:.1f} cm; {sum(1 for s in snaps if s >= 10)} over 10 cm, {sum(1 for s in snaps if s >= 50)} over 50 cm")
    for name in ("client0", "host"):
        count, over_100, worst = stalls(Path(folder) / f"{name}.log")
        lines.append(f"  {name} frames: {count} over 50 ms, {over_100} over 100 ms, worst {worst * 1000:.0f} ms")
    groups = {}
    for row in rows:
        groups.setdefault(classify(row), []).append(row["cm"])
    for cause, values in sorted(groups.items(), key=lambda kv: -len(kv[1])):
        lines.append(f"  {cause:22s} {len(values):4d}  largest {max(values):6.1f} cm  "
                     f"median {sorted(values)[len(values) // 2]:6.1f} cm")
    return "\n".join(lines)


if __name__ == "__main__":
    for name in sys.argv[1:]:
        print(report(name))
