"""Runs the standard moving-ship test matrix, one scenario at a time, and prints every report.

Scenarios run sequentially so they do not steal CPU from each other and distort frame times.
"""
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).parent
SLOW_DISK = HERE.parents[1] / "Saved" / "MotionTests"

SCENARIOS = [
    ["--name", "control_static", "--static-ship"],
    ["--name", "moving_lan"],
    ["--name", "moving_rtt100", "--rtt", "100", "--jitter", "10", "--loss", "1"],
    ["--name", "moving_rtt200", "--rtt", "200", "--jitter", "20", "--loss", "3"],
    ["--name", "moving_shipgravity", "--gravity", "ship"],
    # Writing results to the project's drive stalls the game, if that is a slow disk. Useful on purpose.
    ["--name", "moving_stalls", "--out", str(SLOW_DISK)],
    ["--name", "crew_4players", "--clients", "3"],
    ["--name", "crew_4players_no_nudge", "--clients", "3", "--no-nudge"],
    ["--name", "crew_apart_static", "--static-ship", "--no-walkers", "--spawn-together"],
    ["--name", "crew_apart_moving", "--no-walkers", "--spawn-together"],
]

failed = 0
for scenario in SCENARIOS:
    failed += subprocess.call([sys.executable, str(HERE / "run_motion_test.py"), *scenario])
sys.exit(1 if failed else 0)
