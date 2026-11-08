"""Runs the moving-ship test: one listen host and N clients as separate headless processes.

Both ends run scripted walkers while the server drives the ship through a fixed motion profile.
Each process writes a telemetry CSV; analyze_motion_test.py compares them.

    python Scripts/build/run_motion_test.py --name rtt100 --rtt 100 --jitter 10 --loss 1
    python Scripts/build/run_motion_test.py --name shipgrav --gravity ship
"""
import argparse
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).parent))
import analyze_corrections  # noqa: E402
import analyze_motion_test  # noqa: E402

MAP = "/Game/TestMaps/L_ShipMotion"
PROFILE_SECONDS = 320  # The longer of the two motion profiles.
STARTUP_ALLOWANCE_SECONDS = 180


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--name", default="baseline", help="result folder under Saved/MotionTests")
    parser.add_argument("--engine", default=r"D:\EPIC\UE_5.8")
    parser.add_argument("--clients", type=int, default=1)
    parser.add_argument("--port", type=int, default=7777)
    parser.add_argument("--rtt", type=int, default=0, help="simulated round trip, ms (split across both directions)")
    parser.add_argument("--jitter", type=int, default=0, help="simulated lag variance per direction, ms")
    parser.add_argument("--loss", type=int, default=0, help="simulated packet loss per direction, percent")
    parser.add_argument("--gravity", choices=["world", "ship"], default="world")
    parser.add_argument("--static-ship", action="store_true", help="control run: same walkers, ship held still")
    parser.add_argument("--solo-walker", action="store_true", help="only the client walks; the host stands aside")
    parser.add_argument("--no-nudge", action="store_true", help="crew pass through each other without being nudged apart")
    parser.add_argument("--spawn-together", action="store_true", help="every player spawns at the same spot")
    parser.add_argument("--no-walkers", action="store_true", help="nobody walks; players only react to each other")
    parser.add_argument("--eva", action="store_true", help="slow ship; first client is a diver, others walk")
    parser.add_argument("--eva-all", action="store_true", help="slow ship; everyone is a diver, sharing the airlock")
    parser.add_argument("--exe", help="test a packaged build's Thalassa.exe instead of the editor")
    parser.add_argument("--sonar", action="store_true", help="survey with the sonar during the test")
    parser.add_argument("--helm", action="store_true", help="the deck, driven from the helm by the first client instead of the script")
    parser.add_argument("--helm-course", choices=["tour", "dive", "rise", "cameras", "heave", "sink", "boost"], default="tour", help="what the scripted helmsman does")
    parser.add_argument("--above-ground", type=float, help="open ocean: start the ship this many metres above the highest ground under its hull")
    parser.add_argument("--ship-at", help="start the ship at X,Y,Z world centimetres")
    parser.add_argument("--deck", action="store_true", help="use the real deck instead of the test rig")
    parser.add_argument("--hull-stretch", type=float, help="stretch the hull lengthwise; walkers only")
    parser.add_argument("--no-jump", action="store_true", help="walkers never jump")
    parser.add_argument("--sprint", action="store_true", help="walkers sprint everywhere")
    parser.add_argument("--slide", action="store_true", help="walkers sprint, and slide every few seconds")
    parser.add_argument("--map", default=MAP, help="the map to play, e.g. /Game/Maps/L_Ocean (from Git Bash, set MSYS_NO_PATHCONV=1)")
    parser.add_argument("--record-audio", type=float, help="the host plays with sound on and records this many seconds of it to Saved/BouncedWavFiles/<name>.wav")
    parser.add_argument("--survey-slot", help="the host loads and saves the sonar chart in this save slot")
    parser.add_argument("--vessel", help="ship Blueprint class to spawn instead of the configured one, e.g. /Game/Ship/BP_X.BP_X_C")
    parser.add_argument("--art-probe", action="store_true", help="attach a solid crate to the ship in the walkers' path")
    parser.add_argument("--out", help="result folder's parent; defaults to the system temp folder")
    parser.add_argument("--server-move-scalar", type=float, help="value for p.NetServerMaxMoveDeltaTimeScalar")
    args = parser.parse_args()

    # Not under the project by default. The test writes constantly, and on a slow disk that stalls
    # the game enough to dominate the results. Use --out to test deliberately under stalls.
    out = (Path(args.out) if args.out else Path(tempfile.gettempdir()) / "ThalassaMotionTests") / args.name
    out.mkdir(parents=True, exist_ok=True)
    for stale in [*out.glob("*.csv"), *out.glob("*.log")]:
        stale.unlink()

    editor = Path(args.engine) / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe"
    project = ROOT / "Thalassa.uproject"
    timeout = PROFILE_SECONDS + STARTUP_ALLOWANCE_SECONDS

    common = [
        "-game", "-nullrhi", "-nosound", "-unattended", "-nosplash", "-NoVerifyGC",
        f"-ThalassaTimeout={timeout}", f"-ThalassaGravity={args.gravity}",
        "-ExecCmds=t.MaxFPS 60" + (f",p.NetServerMaxMoveDeltaTimeScalar {args.server_move_scalar}" if args.server_move_scalar else ""),
    ]
    if args.no_nudge:
        common.append("-ThalassaNoNudge")
    if args.spawn_together:
        common.append("-ThalassaSpawnTogether")
    if args.eva or args.eva_all:
        common.append("-ThalassaEvaProfile")
    if args.sonar:
        common.append("-ThalassaSonar")
    if args.helm:
        common += ["-ThalassaHelmTest", f"-ThalassaHelmCourse={args.helm_course}"]
    if args.above_ground:
        common.append(f"-ThalassaShipAboveGround={args.above_ground}")
    if args.ship_at:
        common.append(f"-ThalassaShipAt={args.ship_at}")
    if not args.deck and not args.helm:
        common.append("-ThalassaTestRig")
    if args.hull_stretch:
        common.append(f"-ThalassaHullStretch={args.hull_stretch}")
    if args.no_jump:
        common.append("-ThalassaNoJump")
    if args.sprint:
        common.append("-ThalassaSprint")
    if args.slide:
        common.append("-ThalassaSlide")
    if args.art_probe:
        common.append("-ThalassaArtProbe")
    if args.survey_slot:
        common.append(f"-ThalassaSurveySlot={args.survey_slot}")
    if args.vessel:
        common.append(f"-ini:Game:[/Script/Thalassa.ThalassaGameMode]:VesselClass={args.vessel}")
    if args.static_ship:
        common.append("-ThalassaStaticShip")
    if args.rtt or args.loss:
        common += [f"PktLag={args.rtt // 2}", f"PktLagVariance={args.jitter}", f"PktLoss={args.loss}"]

    def launch(role, url, extra):
        target = [args.exe] if args.exe else [str(editor), str(project)]
        flags = [f for f in common if not (args.record_audio and role == "host" and f == "-nosound")]
        command = [*target, url, *flags, *extra,
                   f"-ThalassaTelemetry={out / (role + '.csv')}", f"-abslog={out / (role + '.log')}"]
        return subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    host_extra = []
    if args.record_audio:
        # A headless game never has focus, and Unreal mutes unfocused games by default.
        host_extra = [f"-ThalassaRecordAudio={args.record_audio}", f"-ThalassaRecordAudioName={args.name}",
                      "-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1.0"]
    processes = {"host": launch("host", f"{args.map}?listen?port={args.port}", [*host_extra,
        f"-ThalassaExpectPlayers={args.clients + 1}", f"-port={args.port}",
        *([] if args.solo_walker or args.no_walkers else ["-ThalassaBot"]),
        *(["-ThalassaDiver"] if args.eva_all else [])])}
    time.sleep(25)  # The host must be listening before clients try to connect.
    for index in range(args.clients):
        processes[f"client{index}"] = launch(f"client{index}", f"127.0.0.1:{args.port}", ["-ThalassaHelmBot"] if args.helm and index == 0 else [] if args.no_walkers else ["-ThalassaBot", *(["-ThalassaDiver"] if args.eva_all or (args.eva and index == 0) else [])])

    deadline = time.time() + timeout + 30
    failed = False
    for role, process in processes.items():
        try:
            code = process.wait(timeout=max(1, deadline - time.time()))
            print(f"{role}: exit code {code}")
            failed = failed or code != 0
        except subprocess.TimeoutExpired:
            print(f"{role}: did not exit in time, killing")
            process.kill()
            failed = True

    text = analyze_motion_test.report(out)
    print(text)
    print(analyze_corrections.report(out))
    print(f"Results in {out}")
    # A clean process exit proves nothing: the test also fails if it produced no measurements.
    failed = failed or "FAILED" in text or "NO SAMPLES" in text
    print("RESULT: " + ("FAILED" if failed else "completed (read the numbers; this is not a pass/fail gate)"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
