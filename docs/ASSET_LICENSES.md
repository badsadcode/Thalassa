# Third-party assets and licences

Everything that ships with the game or the site and was not written for this project.

## Game

| Asset | Source | Licence | Notes |
|---|---|---|---|
| `/Engine/BasicShapes/*` | Unreal Engine | Unreal Engine EULA | Placeholder graybox meshes, referenced from the engine rather than copied. |
| `Content/World/Textures/dark_rock_02/*` (Dark Rock 02) | [Poly Haven](https://polyhaven.com/a/dark_rock_02) | CC0 1.0 | 2K colour, normal, AO/roughness. Normal flipped from OpenGL on import. Used by `M_Rock`. |
| `Content/World/Textures/coral_gravel/*` (Coral Gravel) | [Poly Haven](https://polyhaven.com/a/coral_gravel) | CC0 1.0 | 2K colour, normal, AO/roughness. Used by `M_SeabedGround` on slopes. |
| `Content/World/Textures/damp_beach_sand/*` (Damp Beach Sand) | [Poly Haven](https://polyhaven.com/a/damp_beach_sand) | CC0 1.0 | 2K colour, normal, AO/roughness. Imported, not used yet. |
| `Content/World/Textures/Ground093C/*` (Ground 093 C) | [ambientCG](https://ambientcg.com/view?id=Ground093C) | CC0 1.0 | 2K colour, DirectX normal, roughness. Used by `M_SeabedGround` on the flats. |

The CC0 sources are fetched into `Saved/Textures/` (not in git) by `Scripts/build/fetch_textures.py`, and imported by `Scripts/unreal/create_ocean_materials.py`. CC0 needs no attribution. The sites are credited here as a courtesy.

## Devlog site

| Asset | Source | Licence | Modified? |
|---|---|---|---|
| Saira | Fontsource | SIL OFL 1.1 | No |
| Source Serif 4 | Fontsource | SIL OFL 1.1 | No |
| JetBrains Mono | Fontsource | SIL OFL 1.1 | No |
| `site/public/images/concept/*` | Cropped from the project's title screen concept (`docs/Tell us a title screen concept.png`) | **Owner to confirm** | Cropped and resized |

**Open question for the owner:** where the title screen concept came from and what rights apply. The site shows it labelled as concept art, on the assumption that the project may use it publicly.
