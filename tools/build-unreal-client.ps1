[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$EngineRoot = $env:UE_5_ROOT,

    [Parameter(Mandatory = $false)]
    [string]$ProjectPath = ""
)

$ErrorActionPreference = "Stop"

if (-not $ProjectPath) {
    $ProjectPath = Join-Path $PSScriptRoot "..\client\AngbandFirstPerson.uproject"
}
$ProjectPath = [System.IO.Path]::GetFullPath($ProjectPath)

if (-not $EngineRoot) {
    $EpicGamesRoot = Join-Path $env:ProgramFiles "Epic Games"
    $InstalledEngines = @()

    if (Test-Path $EpicGamesRoot) {
        $InstalledEngines = Get-ChildItem -Path $EpicGamesRoot -Directory |
            Where-Object { $_.Name -match '^UE_[56]\.' } |
            Sort-Object {
                [version]($_.Name -replace '^UE_', '')
            } -Descending
    }

    foreach ($InstalledEngine in $InstalledEngines) {
        $CandidateBuildBat = Join-Path `
            $InstalledEngine.FullName `
            "Engine\Build\BatchFiles\Build.bat"

        if (Test-Path $CandidateBuildBat) {
            $EngineRoot = $InstalledEngine.FullName
            break
        }
    }
}

if (-not $EngineRoot) {
    throw "Unreal Engine 5 was not found. Pass -EngineRoot or set UE_5_ROOT to its installation directory."
}

$EngineRoot = [System.IO.Path]::GetFullPath($EngineRoot)

$BatchFilesPath = Join-Path $EngineRoot "Engine\Build\BatchFiles"
$BuildBat = Join-Path $BatchFilesPath "Build.bat"

if (-not (Test-Path $BuildBat)) {
    throw "Build.bat was not found under EngineRoot '$EngineRoot'."
}

if (-not (Test-Path $ProjectPath)) {
    throw "Unreal project was not found: $ProjectPath"
}

Write-Host "Building AngbandFirstPersonEditor for Win64 Development"
Write-Host "Engine:  $EngineRoot"
Write-Host "Project: $ProjectPath"
Write-Host "Working directory: $BatchFilesPath"

Push-Location $BatchFilesPath

try {
    & $BuildBat `
        "AngbandFirstPersonEditor" `
        "Win64" `
        "Development" `
        "-Project=$ProjectPath" `
        "-WaitMutex" `
        "-FromMsBuild"

    if ($LASTEXITCODE -ne 0) {
        throw "UnrealBuildTool failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

Write-Host "Unreal editor target compiled successfully."