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
    Write-Host "  1. Open the project in Unreal Editor (ensure Python Editor Script Plugin is enabled)."
    Write-Host "  2. Run: Tools -> Execute Python Script -> $ImportScript"
    Write-Host "  3. Uncomment VisualCatalogAsset in client/Config/DefaultGame.ini and press Play."
    Write-Host ""
    Write-Host "Or re-run this script with -AutoImport to invoke UnrealEditor-Cmd.exe headlessly."
    exit 0
}

if (-not $ProjectPath) {
    $ProjectPath = Join-Path $RepoRoot "client\AngbandFirstPerson.uproject"
}
$ProjectPath = [System.IO.Path]::GetFullPath($ProjectPath)

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

Write-Host "Running headless Unreal asset import..."
& $EditorCmd $ProjectPath "-ExecutePythonScript=$ImportScript" "-nop4" "-unattended" "-nosplash"
if ($LASTEXITCODE -ne 0) {
    throw "UnrealEditor-Cmd import failed with exit code $LASTEXITCODE."
}
Write-Host "Asset import complete! Uncomment VisualCatalogAsset in client/Config/DefaultGame.ini to activate."
