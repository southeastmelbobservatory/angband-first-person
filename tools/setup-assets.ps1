[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$EngineRoot = $env:UE_5_ROOT,

    [Parameter(Mandatory = $false)]
    [string]$ProjectPath = "",

    [Parameter(Mandatory = $false)]
    [switch]$AutoImport
)

$ErrorActionPreference = "Stop"

$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$FetchScript = Join-Path $PSScriptRoot "fetch-assets.py"
$ImportScript = Join-Path $PSScriptRoot "import-assets-to-unreal.py"

$PythonCmd = Get-Command python -ErrorAction SilentlyContinue
if (-not $PythonCmd) { $PythonCmd = Get-Command python3 -ErrorAction SilentlyContinue }
if (-not $PythonCmd) { $PythonCmd = Get-Command py -ErrorAction SilentlyContinue }
if (-not $PythonCmd) {
    throw "Python 3 was not found in PATH. Install Python 3 or run tools/fetch-assets.py inside WSL."
}

Write-Host "Downloading and verifying CC0 assets into assets/raw/..."
& $PythonCmd.Source $FetchScript
if ($LASTEXITCODE -ne 0) {
    throw "Asset fetch failed with exit code $LASTEXITCODE."
}

if (-not $AutoImport) {
    Write-Host ""
    Write-Host "Assets are ready in assets/raw/."
    Write-Host "To auto-import all meshes, PBR materials, audio, and populate DA_DungeonVisualCatalog:"
    Write-Host "  .\tools\setup-assets.ps1 -AutoImport"
    Write-Host "  (or pass -ProjectPath `"<path-to-AngbandFirstPerson.uproject>`")"
    exit 0
}

if (-not $ProjectPath) {
    $ProjectPath = Join-Path $RepoRoot "client\AngbandFirstPerson.uproject"
}
$ProjectPath = [System.IO.Path]::GetFullPath($ProjectPath)
if (-not (Test-Path $ProjectPath)) {
    throw "Unreal project was not found: $ProjectPath"
}

if (-not $EngineRoot) {
    $EpicGamesRoot = Join-Path $env:ProgramFiles "Epic Games"
    if (Test-Path $EpicGamesRoot) {
        $Installed = Get-ChildItem -Path $EpicGamesRoot -Directory |
            Where-Object { $_.Name -match '^UE_[56]\.' } |
            Sort-Object { [version]($_.Name -replace '^UE_', '') } -Descending
        foreach ($Dir in $Installed) {
            $CmdExe = Join-Path $Dir.FullName "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
            if (Test-Path $CmdExe) {
                $EngineRoot = $Dir.FullName
                break
            }
        }
    }
}

if (-not $EngineRoot) {
    throw "Unreal Engine was not found. Pass -EngineRoot to use -AutoImport."
}

$EditorCmd = Join-Path ([System.IO.Path]::GetFullPath($EngineRoot)) "Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
if (-not (Test-Path $EditorCmd)) {
    throw "UnrealEditor-Cmd.exe not found at $EditorCmd"
}

$env:ABFP_REPO_ROOT = $RepoRoot
$LogDir = Join-Path $RepoRoot "Saved\Logs"
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$ImportLog = Join-Path $LogDir "asset-import.log"

Write-Host "Running headless Unreal Python importer against $ProjectPath..."
& $EditorCmd $ProjectPath "-run=pythonscript" "-script=$ImportScript" "-abslog=$ImportLog" "-stdout" "-FullStdOutLogOutput" "-nop4" "-unattended" "-nosplash"
$ImportExit = $LASTEXITCODE

if (Test-Path $ImportLog) {
    Write-Host ""
    Write-Host "=== [ABFP] Import Summary ===" -ForegroundColor Cyan
    Select-String -Path $ImportLog -Pattern "\[ABFP\] (Creature species imported|Default creature|Reduced |Mesh reduction|Saved /Game/Data|No mesh found|Failed importing|Failed building)" | ForEach-Object { Write-Host $_.Line }
}

if ($ImportExit -ne 0) {
    throw "UnrealEditor-Cmd import failed with exit code $ImportExit. Full log: $ImportLog"
}

$ProjectDir = Split-Path -Parent $ProjectPath
$CatalogFile = Join-Path $ProjectDir "Content\Data\DA_DungeonVisualCatalog.uasset"
if (-not (Test-Path $CatalogFile)) {
    throw "Import command exited 0, but $CatalogFile was not created. Check the log lines above for [ABFP] errors (ensure AngbandFirstPerson C++ is compiled and PythonScriptPlugin is enabled in $ProjectPath)."
}

Write-Host ""
Write-Host "Asset import verified! Created: $CatalogFile" -ForegroundColor Green
