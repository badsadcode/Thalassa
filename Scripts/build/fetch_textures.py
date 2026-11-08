"""Downloads the CC0 sources for the world: textures into Saved/Textures/<asset>/, and scanned rock
models (glTF with their textures) into Saved/Models/<asset>/.

The sources stay out of git: they can always be fetched again. Only the imported Unreal textures are
committed. Every asset is CC0 (public domain) and listed in docs/ASSET_LICENSES.md.

    python Scripts/build/fetch_textures.py

Poly Haven files come from its public API; ambientCG sends one ZIP per material, from which only the
image files are kept.
"""
import io
import json
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "Saved" / "Textures"
AGENT = {"User-Agent": "ThalassaDev/1.0"}

POLY_HAVEN = ["dark_rock_02", "damp_beach_sand", "coral_gravel"]
POLY_HAVEN_MAPS = {"Diffuse": "color", "nor_gl": "normal", "arm": "arm"}  # arm: ambient occlusion, roughness, metal.
AMBIENT_CG = ["Ground093C"]
POLY_HAVEN_MODELS = ["moon_rock_01", "moon_rock_03", "moon_rock_05", "rock_07", "rock_09", "namaqualand_boulder_03",
                     "namaqualand_boulder_05", "coast_rocks_05"]
MODELS = ROOT / "Saved" / "Models"


def get(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=AGENT), timeout=120) as response:
        return response.read()


def main():
    for asset in POLY_HAVEN:
        folder = OUT / asset
        folder.mkdir(parents=True, exist_ok=True)
        files = json.loads(get(f"https://api.polyhaven.com/files/{asset}"))
        for key, role in POLY_HAVEN_MAPS.items():
            info = files[key]["2k"]["jpg"]
            target = folder / f"{asset}_{role}.jpg"
            if not target.exists():
                target.write_bytes(get(info["url"]))
            print(f"{asset}: {target.name} {target.stat().st_size / 1e6:.1f} MB")

    for asset in AMBIENT_CG:
        folder = OUT / asset
        folder.mkdir(parents=True, exist_ok=True)
        if not any(folder.glob("*.jpg")):
            archive = zipfile.ZipFile(io.BytesIO(get(f"https://ambientcg.com/get?file={asset}_2K-JPG.zip")))
            for member in archive.namelist():
                name = Path(member).name  # Never trust paths inside an archive.
                if name.lower().endswith((".jpg", ".png")) and "/" not in member.strip("/") and not name.startswith("."):
                    (folder / name).write_bytes(archive.read(member))
        for image in sorted(folder.glob("*.jpg")):
            print(f"{asset}: {image.name} {image.stat().st_size / 1e6:.1f} MB")

    for asset in POLY_HAVEN_MODELS:
        folder = MODELS / asset
        files = json.loads(get(f"https://api.polyhaven.com/files/{asset}"))
        gltf = files["gltf"]["2k"]["gltf"]
        wanted = [(f"{asset}.gltf", gltf["url"])] + [(relative, info["url"]) for relative, info in gltf["include"].items()]
        total = 0
        for relative, url in wanted:
            target = (folder / relative).resolve()
            if folder.resolve() not in target.parents:
                raise RuntimeError(f"refusing a path outside the asset folder: {relative}")
            target.parent.mkdir(parents=True, exist_ok=True)
            if not target.exists():
                target.write_bytes(get(url))
            total += target.stat().st_size
        print(f"{asset}: {len(wanted)} files, {total / 1e6:.1f} MB")


if __name__ == "__main__":
    main()
