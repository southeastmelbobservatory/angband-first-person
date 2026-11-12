[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$EngineRoot = $env:UE_5_ROOT
)

$ErrorActionPreference = "Stop"
$ProjectPath = [System.IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot "..\client\AngbandFirstPerson.uproject")
)

if (-not $EngineRoot) {
    $EpicGamesRoot = Join-Path $env:ProgramFiles "Epic Games"
    $InstalledEngines = @()
    if (Test-Path $EpicGamesRoot) {
        $InstalledEngines = Get-ChildItem -Path $EpicGamesRoot -Directory -Filter "UE_5.*" |
            Sort-Object Name -Descending
    }

    foreach ($InstalledEngine in $InstalledEngines) {
        $CandidateBuildBat = Join-Path $InstalledEngine.FullName "Engine\Build\BatchFiles\Build.bat"
        if (Test-Path $CandidateBuildBat) {
            $EngineRoot = $InstalledEngine.FullName
            break
        }
    }
}

if (-not $EngineRoot) {
    throw "Unreal Engine 5 was not found. Pass -EngineRoot or set UE_5_ROOT to its installation directory."
}

$BuildBat = Join-Path $EngineRoot "Engine\Build\BatchFiles\Build.bat"
if (-not (Test-Path $BuildBat)) {
    throw "Build.bat was not found under EngineRoot '$EngineRoot'."
}
if (-not (Test-Path $ProjectPath)) {
    throw "Unreal project was not found: $ProjectPath"
}

Write-Host "Building AngbandFirstPersonEditor for Win64 Development"
Write-Host "Engine:  $EngineRoot"
Write-Host "Project: $ProjectPath"

& $BuildBat "AngbandFirstPersonEditor" "Win64" "Development" "-Project=$ProjectPath" "-WaitMutex" "-FromMsBuild"
if ($LASTEXITCODE -ne 0) {
    throw "UnrealBuildTool failed with exit code $LASTEXITCODE."
}

Write-Host "Unreal editor target compiled successfully."
