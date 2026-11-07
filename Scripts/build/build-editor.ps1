# Builds the Thalassa editor target (Development, Win64).
param([string]$EngineRoot = "D:\EPIC\UE_5.8")
$project = (Resolve-Path "$PSScriptRoot\..\..\Thalassa.uproject").Path
& "$EngineRoot\Engine\Build\BatchFiles\Build.bat" ThalassaEditor Win64 Development "-Project=$project" -WaitMutex
exit $LASTEXITCODE
