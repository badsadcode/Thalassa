# Builds a packaged Windows copy of THALASSA that runs without the editor.
#
#   powershell -NoProfile -File Scripts/build/package-windows.ps1
#
# The result is a folder you can copy to another PC: <Output>\Windows\Thalassa.exe, plus the
# Host and Join launchers from Scripts/build/launchers. Defaults to D:\ThalassaBuilds, an SSD,
# because the project's own drive is slow.
param(
    [string]$EngineRoot = "D:\EPIC\UE_5.8",
    [string]$Output = "D:\ThalassaBuilds",
    [string]$Configuration = "Development"
)

$project = (Resolve-Path "$PSScriptRoot\..\..\Thalassa.uproject").Path
& "$EngineRoot\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun `
    "-project=$project" -platform=Win64 "-clientconfig=$Configuration" `
    -build -cook -stage -pak -archive "-archivedirectory=$Output" `
    -unattended -utf8output -nop4
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Copy-Item "$PSScriptRoot\launchers\*" "$Output\Windows\" -Force
Write-Host "Packaged to $Output\Windows"
