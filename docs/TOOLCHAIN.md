# Toolchain

What this project was actually built and tested with. Update this when a version changes.

| Tool | Version | Notes |
|---|---|---|
| Unreal Engine | 5.8.4 (CL 58902052) | Launcher install. Project is associated with `5.8`. |
| Visual Studio | 2022 Community 17.14 | |
| MSVC | 14.44.35229 | Its folder is named `14.44.35207`. UE 5.8 bans 14.44 below 35211, but goes by the real compiler version, so this one is accepted. |
| Windows SDK | 10.0.22621 | Selected by UnrealBuildTool. |
| Git / Git LFS | 2.50.1 / 3.7.0 | |
| Python | 3.12 | Test scripts. Standard library only. |
| Node.js / npm | 24.7 / 11.5 | Devlog site only. |

Development machine: Ryzen 9 5950X, 32 GB RAM, RTX 3080.

## Unreal plugins

| Plugin | Used for | Scope | Origin |
|---|---|---|---|
| Enhanced Input | Player controls | Runtime | Built in |
| Python Editor Script Plugin | Generating the test map | Editor only | Built in |
| Editor Scripting Utilities | Editor automation | Editor only | Built in |
| Unreal MCP (`ModelContextProtocol`) | Lets Claude Code work in the open editor | Editor only | Built in, experimental |
| All Toolsets | The tools the MCP server offers | Editor only | Built in, experimental |

No third-party plugins.

## Automation bridges

**Unreal MCP**, Epic's own MCP server, ships with UE 5.8 as an experimental plugin, and it is enabled here for the editor only. It starts with the editor (`Config/DefaultEditorPerProjectUserSettings.ini`) and listens on this machine only, at `http://127.0.0.1:8000/mcp`. It has no authentication, so it must never be exposed beyond this PC. Epic's usage analytics for it are turned off in `DefaultEngine.ini`.

`.mcp.json` in the project root points Claude Code at it. A Claude Code session started in the project folder while the editor is running can then use the editor's tools: spawning and editing actors, materials, scenes, and running automation tests. Tool calls run one at a time on the editor's game thread. Epic's guide: https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor

Before this, everything ran through ordinary command-line tools and Unreal's own editor Python, and those still work. Compiling C++ still needs the editor closed. No Blender MCP is configured. Blender 5.1 is installed on the development machine but has not been used yet.

## Machine-specific notes

- The project is on `H:`, a slow WD Green hard drive. Heavy writes there stalled the game during tests. Consider moving the project to an SSD (`D:` has space).
- The Windows registry still lists a UE 5.6 install at `D:\EPIC\UE_5.6`. That folder holds no engine, so ignore it.
- Very long shell commands can fail outright on Windows, so prefer scripts over one-liners.
