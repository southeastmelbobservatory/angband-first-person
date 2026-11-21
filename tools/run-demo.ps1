[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$EngineRoot = $env:UE_5_ROOT,

    [Parameter(Mandatory = $false)]
    [string]$ProjectPath = "",

    [Parameter(Mandatory = $false)]
    [string]$WslDistro = "",

    [Parameter(Mandatory = $false)]
    [string]$BindAddress = "127.0.0.1",

    [Parameter(Mandatory = $false)]
    [switch]$SkipBuild,

    [Parameter(Mandatory = $false)]
    [switch]$NoEditor
)

$ErrorActionPreference = "Stop"

$RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if (-not $ProjectPath) {
    $ProjectPath = Join-Path $RepoRoot "client\AngbandFirstPerson.uproject"
}
$ProjectPath = [System.IO.Path]::GetFullPath($ProjectPath)

if (-not (Test-Path $ProjectPath)) {
    throw "Unreal project was not found: $ProjectPath"
}

function Resolve-UnrealEngineRoot {
    param([string]$RequestedRoot)

    if ($RequestedRoot) {
        return [System.IO.Path]::GetFullPath($RequestedRoot)
    }

    $EpicGamesRoot = Join-Path $env:ProgramFiles "Epic Games"
    if (Test-Path $EpicGamesRoot) {
        $InstalledEngines = Get-ChildItem -Path $EpicGamesRoot -Directory |
            Where-Object { $_.Name -match '^UE_[56]\.' } |
            Sort-Object {
                [version]($_.Name -replace '^UE_', '')
            } -Descending

        foreach ($InstalledEngine in $InstalledEngines) {
            $CandidateEditor = Join-Path $InstalledEngine.FullName "Engine\Binaries\Win64\UnrealEditor.exe"
            if (Test-Path $CandidateEditor) {
                return [System.IO.Path]::GetFullPath($InstalledEngine.FullName)
            }
        }
    }

    throw "Unreal Engine was not found. Pass -EngineRoot or set UE_5_ROOT to your installation directory."
}

$WslCmd = Get-Command wsl.exe -ErrorAction SilentlyContinue
if (-not $WslCmd) {
    throw "wsl.exe was not found. Install WSL (wsl --install) or run tools/run-native-demo.sh on a Linux host."
}

if (-not $SkipBuild) {
    $BuildScript = Join-Path $PSScriptRoot "build-unreal-client.ps1"
    $BuildParams = @{ ProjectPath = $ProjectPath }
    if ($EngineRoot) {
        $BuildParams["EngineRoot"] = $EngineRoot
    }
    Write-Host "Building Unreal client C++ module..."
    & $BuildScript @BuildParams
}

$EditorExe = $null
if (-not $NoEditor) {
    $ResolvedEngineRoot = Resolve-UnrealEngineRoot -RequestedRoot $EngineRoot
    $EditorExe = Join-Path $ResolvedEngineRoot "Engine\Binaries\Win64\UnrealEditor.exe"
    if (-not (Test-Path $EditorExe)) {
        throw "UnrealEditor.exe was not found under '$ResolvedEngineRoot'."
    }
}

$WslPrefixArgs = @()
if ($WslDistro) {
    $WslPrefixArgs += @("-d", $WslDistro)
}

# Convert Windows repo path to its WSL mount path using wslpath.
$ForwardSlashRoot = $RepoRoot.Replace("\", "/")
$WslRepoPath = (& wsl.exe @WslPrefixArgs wslpath -a -u "$ForwardSlashRoot").Trim()
if ($LASTEXITCODE -ne 0 -or -not $WslRepoPath) {
    throw "Failed to resolve WSL path for '$RepoRoot'."
}

$WslHostScript = "$WslRepoPath/tools/run-wsl-host.sh"
Write-Host "Starting Angband native host in WSL ($WslHostScript) on ${BindAddress}:47652..."

$WslLaunchArgs = $WslPrefixArgs + @("--", "env", "ABFP_BIND_ADDRESS=$BindAddress", "bash", $WslHostScript)

$WtCmd = Get-Command wt.exe -ErrorAction SilentlyContinue
if ($WtCmd) {
    $WtArgs = @("new-tab", "--title", "Angband Host (ABFP)", "wsl.exe") + $WslLaunchArgs
    Start-Process -FilePath $WtCmd.Source -ArgumentList $WtArgs
} else {
    Start-Process -FilePath $WslCmd.Source -ArgumentList $WslLaunchArgs
}

if (-not $NoEditor) {
    Write-Host "Launching Unreal Editor: $ProjectPath"
    Start-Process -FilePath $EditorExe -ArgumentList @("`"$ProjectPath`"")
}

Write-Host ""
Write-Host "Next steps:"
Write-Host "  1. Finish Angband character creation/load in the WSL terminal window."
Write-Host "  2. Once Angband enters the town/dungeon, press Play in Unreal Editor."
Write-Host "  3. The HUD will switch automatically to ANGBAND LIVE (${BindAddress}:47652)."
