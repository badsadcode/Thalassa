# Third-party assets and licences

Everything that ships with the game or the site and was not written for this project.

## Game

| Asset | Source | Licence | Notes |
|---|---|---|---|
| `/Engine/BasicShapes/*` | Unreal Engine | Unreal Engine EULA | Placeholder graybox meshes, referenced from the engine rather than copied. |
| `Content/World/Textures/dark_rock_02/*` (Dark Rock 02) | [Poly Haven](https://polyhaven.com/a/dark_rock_02) | CC0 1.0 | 2K colour, normal, AO/roughness. Normal flipped from OpenGL on import. Used by `M_Rock`. |
| `Content/World/Textures/coral_gravel/*` (Coral Gravel) | [Poly Haven](https://polyhaven.com/a/coral_gravel) | CC0 1.0 | 2K colour, normal, AO/roughness. Used by `M_SeabedGround` on slopes. |
| `Content/World/Textures/damp_beach_sand/*` (Damp Beach Sand) | [Poly Haven](https://polyhaven.com/a/damp_beach_sand) | CC0 1.0 | 2K colour, normal, AO/roughness. Imported, not used yet. |
| `Content/World/Rocks/moon_rock_01/*` | [Poly Haven](https://polyhaven.com/a/moon_rock_01) | CC0 1.0 | Scanned rock, 2K textures. Scattered at many sizes. |
| `Content/World/Rocks/moon_rock_03/*` | [Poly Haven](https://polyhaven.com/a/moon_rock_03) | CC0 1.0 | Scanned rock, 2K textures. |
| `Content/World/Rocks/moon_rock_05/*` | [Poly Haven](https://polyhaven.com/a/moon_rock_05) | CC0 1.0 | Scanned rock, 2K textures. |
| `Content/World/Rocks/rock_07/*` | [Poly Haven](https://polyhaven.com/a/rock_07) | CC0 1.0 | Scanned rock, 2K textures. |
| `Content/World/Rocks/rock_09/*` | [Poly Haven](https://polyhaven.com/a/rock_09) | CC0 1.0 | Scanned flat stone, 2K textures. |
| `Content/World/Rocks/namaqualand_boulder_03/*` | [Poly Haven](https://polyhaven.com/a/namaqualand_boulder_03) | CC0 1.0 | Scanned boulder, 2K textures. |
| `Content/World/Rocks/namaqualand_boulder_05/*` | [Poly Haven](https://polyhaven.com/a/namaqualand_boulder_05) | CC0 1.0 | Scanned boulder, 2K textures. |
| `Content/World/Rocks/coast_rocks_05/*` | [Poly Haven](https://polyhaven.com/a/coast_rocks_05) | CC0 1.0 | Scanned coastal rock mass, 2K textures. Used at 16 to 32 m as medium landmarks. |
| `Content/World/Meshy/*` (rocky cliff ledge, weathered stone arch, stone cluster, stone monolith) | Made by the owner with [Meshy AI](https://www.meshy.ai) | Owner's paid Meshy subscription, which allows commercial use | Slimmed to 30,000 triangles and 2K textures (`slim_meshy.py`), imported by `import_meshy.py`. Coloured by our Dark Rock 02 scan by default. |
| `Content/Creatures/Eyes/*` (iris textures) | Made by the owner | Owner's own | Green slit (leviathan), orange horizontal slit (kraken); an amber round one is kept for later. |
| `Content/Hallway_sci_fi/*` (sci-fi hallway kit) | The owner's [Fab](https://www.fab.com) library | Fab licence, as acquired by the owner | Dresses the aft corridor (`Content/Ship/Art/L_Art_AftCorridor`). Not in git: Fab assets must never go to the public repo. |
| Fab sea creatures (`Content/Shark_Megalodon`, `BlueWhaleAndWhaleCalf`, `HammerheadShark` and the rest) | The owner's [Fab](https://www.fab.com) library | Fab licence, as acquired by the owner | Animated creatures. The game tints them through `ThalassaTint` (`tint_fab_materials.py`). Not in git. |
| `Content/World/Textures/Ground093C/*` (Ground 093 C) | [ambientCG](https://ambientcg.com/view?id=Ground093C) | CC0 1.0 | 2K colour, DirectX normal, roughness. Used by `M_SeabedGround` on the flats. |

The CC0 sources are fetched into `Saved/Textures/` and `Saved/Models/` (not in git) by `Scripts/build/fetch_textures.py`. They're imported by `Scripts/unreal/create_ocean_materials.py` and `Scripts/unreal/import_rock_models.py`. The rock models' colours are cooled on import (BaseColorFactor). CC0 needs no attribution. The sites are credited here as a courtesy.

## Devlog site

| Asset | Source | Licence | Modified? |
|---|---|---|---|
| Saira | Fontsource | SIL OFL 1.1 | No |
| Source Serif 4 | Fontsource | SIL OFL 1.1 | No |
| JetBrains Mono | Fontsource | SIL OFL 1.1 | No |
| `site/public/images/concept/*` | Cropped from the project's title screen concept (`docs/Tell us a title screen concept.png`) | **Owner to confirm** | Cropped and resized |

**Open question for the owner:** where the title screen concept came from and what rights apply. The site shows it labelled as concept art, on the assumption that the project may use it publicly.
