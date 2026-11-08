"""Runs a Blender script headless with the newest Blender installed, so no script names a Blender version.

    python Scripts/build/blender.py Scripts/blender/make_seabed.py -- <arguments for the script>

Set the BLENDER environment variable to a blender.exe to use a particular one instead.
"""
import os
import re
import subprocess
import sys
from pathlib import Path


def find_blender():
    chosen = os.environ.get("BLENDER")
    if chosen:
        return Path(chosen)
    root = Path(r"C:\Program Files\Blender Foundation")
    versions = []
    for folder in root.glob("Blender *"):
        match = re.match(r"Blender (\d+)\.(\d+)", folder.name)
        if match and (folder / "blender.exe").exists():
            versions.append(((int(match.group(1)), int(match.group(2))), folder / "blender.exe"))
    if not versions:
        raise SystemExit("No Blender found in %s. Set BLENDER to a blender.exe." % root)
    return max(versions)[1]


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    blender = find_blender()
    print("Blender: %s" % blender)
    command = [str(blender), "--background", "--factory-startup", "--python", sys.argv[1], *sys.argv[2:]]
    raise SystemExit(subprocess.run(command).returncode)


if __name__ == "__main__":
    main()
