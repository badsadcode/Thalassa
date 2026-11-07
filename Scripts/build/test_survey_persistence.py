"""Checks that the sonar chart survives a restart, and reaches a player who joins afterwards.

1. A game surveys briefly with the sonar on, then quits, saving the chart.
2. A new game starts with the sonar off, and a client joins it.
3. Both should show the same chart as the first game saved.

Uses its own save slot, so a real chart is never touched.
"""
import csv
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EDITOR = r"D:\EPIC\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
PROJECT = str(ROOT / "Thalassa.uproject")
SLOT = "PersistenceTest"
OUT = Path(tempfile.gettempdir()) / "ThalassaPersistenceTest"
COMMON = ["-game", "-nullrhi", "-nosound", "-unattended", "-nosplash", "-ExecCmds=t.MaxFPS 60", "-ThalassaTestRig",
          f"-ThalassaSurveySlot={SLOT}"]


def launch(name, url, extra):
    return subprocess.Popen([EDITOR, PROJECT, url, *COMMON, *extra, f"-ThalassaTelemetry={OUT / (name + '.csv')}",
                             f"-abslog={OUT / (name + '.log')}"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def final_chart(name):
    rows = list(csv.DictReader(open(OUT / f"{name}.csv")))
    return (int(rows[-1]["sonar_cells"]), int(rows[-1]["sonar_sum"])) if rows else (0, 0)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for stale in OUT.glob("*"):
        stale.unlink()
    (ROOT / "Saved" / "SaveGames" / f"{SLOT}.sav").unlink(missing_ok=True)

    first = launch("survey", "/Game/TestMaps/L_ShipMotion", ["-ThalassaSonar", "-ThalassaTimeout=20"])
    first.wait(timeout=240)
    surveyed = final_chart("survey")
    saved = (ROOT / "Saved" / "SaveGames" / f"{SLOT}.sav").exists()
    print(f"first game: surveyed {surveyed[0]} cells, checksum {surveyed[1]}; save file written: {saved}")

    host = launch("host", "/Game/TestMaps/L_ShipMotion?listen?port=7795", ["-port=7795", "-ThalassaTimeout=60"])
    time.sleep(25)
    client = launch("client", "127.0.0.1:7795", ["-ThalassaTimeout=30"])
    host.wait(timeout=240)
    client.wait(timeout=240)
    for name in ("host", "client"):
        cells, checksum = final_chart(name)
        print(f"after restart, {name}: {cells} cells, checksum {checksum}")

    ok = surveyed[0] > 0 and saved and final_chart("host") == surveyed and final_chart("client") == surveyed
    print("RESULT:", "chart survived the restart and reached the client" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
