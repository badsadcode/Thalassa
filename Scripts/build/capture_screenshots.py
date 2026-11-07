"""Captures rendered frames from aboard the ship, one per view direction.

Launches from Python on purpose: Git Bash rewrites "/Game/..." arguments into Windows paths.

    python Scripts/build/capture_screenshots.py Saved/Screenshots/spike console=0 window=-90 corridor=180

A view whose name starts with "airlock" begins in the corridor, in front of the airlock.
A view whose name starts with "sonar" switches the sonar on and opens the station view.
A view whose name starts with "screen" switches the sonar on and leaves the view clear, to see the bridge screen.
Add @X,Y,Z to a view to start there, in ship coordinates: bridge=0@10300,-300,120
"""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EDITOR = Path(r"D:\EPIC\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe")
MAP = "/Game/TestMaps/L_ShipMotion"
TIMEOUT_SECONDS = 280


def main():
    out = Path(sys.argv[1]).resolve()
    out.mkdir(parents=True, exist_ok=True)
    failed = False
    for view in sys.argv[2:]:
        name, setting = view.split("=")
        yaw, _, spawn = setting.partition("@")
        target = out / f"{name}.png"
        target.unlink(missing_ok=True)
        subprocess.run(
            [str(EDITOR), str(ROOT / "Thalassa.uproject"), MAP, "-game", "-windowed", "-ResX=1280", "-ResY=720",
             "-nosound", "-unattended", "-nosplash", f"-ThalassaScreenshot={target}", f"-ThalassaLookYaw={yaw}",
             f"-abslog={out / (name + '.log')}",
             *(["-ThalassaSpawnAtAirlock"] if name.startswith("airlock") else []),
             *([f"-ThalassaSpawnAt={spawn}"] if spawn else []),
             *(["-ThalassaSonar", "-ThalassaStationView"] if name.startswith("sonar") else []),
             *(["-ThalassaSonar"] if name.startswith("screen") else [])],
            timeout=TIMEOUT_SECONDS, check=False)
        exists = target.exists()
        print(f"{name}: {'saved ' + str(target) if exists else 'NO IMAGE'}")
        failed = failed or not exists
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
