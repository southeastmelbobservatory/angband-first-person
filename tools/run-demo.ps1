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
    [string]$PlayerName = "ABFPPlayer",

    [Parameter(Mandatory = $false)]
    [switch]$InteractiveHost,

    [Parameter(Mandatory = $false)]
    [switch]$NewGame,

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

function Test-TcpPortOpen {
    param([string]$TargetHost, [int]$Port)
    $Client = New-Object System.Net.Sockets.TcpClient
    try {
        $Async = $Client.BeginConnect($TargetHost, $Port, $null, $null)
        if ($Async.AsyncWaitHandle.WaitOne(250, $false)) {
            $Client.EndConnect($Async)
            return $true
        }
        return $false
    }
    catch {
        return $false
    }
    finally {
        $Client.Close()
    }
}

$WslCmd = Get-Command wsl.exe -ErrorAction SilentlyContinue
if (-not $WslCmd) {
    throw "wsl.exe was not found. Install WSL (wsl --install) or run tools/run-angband-headless.py on a Linux host."
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

$ForwardSlashRoot = $RepoRoot.Replace("\", "/")
$WslRepoPath = (& wsl.exe @WslPrefixArgs wslpath -a -u "$ForwardSlashRoot").Trim()
if ($LASTEXITCODE -ne 0 -or -not $WslRepoPath) {
    throw "Failed to resolve WSL path for '$RepoRoot'."
}

$ProbeHost = if ($BindAddress -eq "0.0.0.0") { "127.0.0.1" } else { $BindAddress }

if (Test-TcpPortOpen -TargetHost $ProbeHost -Port 47652) {
    Write-Host "Angband host is already running and listening on ${ProbeHost}:47652."
}
elseif ($InteractiveHost) {
    $WslHostScript = "$WslRepoPath/tools/run-wsl-host.sh"
    Write-Host "Starting interactive Angband host in WSL ($WslHostScript) on ${BindAddress}:47652..."
    $WslLaunchArgs = $WslPrefixArgs + @("--", "env", "ABFP_BIND_ADDRESS=$BindAddress", "bash", $WslHostScript)
    $WtCmd = Get-Command wt.exe -ErrorAction SilentlyContinue
    if ($WtCmd) {
        $WtArgs = @("new-tab", "--title", "Angband Host (ABFP)", "wsl.exe") + $WslLaunchArgs
        Start-Process -FilePath $WtCmd.Source -ArgumentList $WtArgs
    } else {
        Start-Process -FilePath $WslCmd.Source -ArgumentList $WslLaunchArgs
    }
}
else {
    $LogDir = Join-Path $RepoRoot "Saved\Logs"
    New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
    $HostLog = Join-Path $LogDir "angband-host.log"
    $HostErrLog = Join-Path $LogDir "angband-host.err.log"

    $HeadlessScript = "$WslRepoPath/tools/run-angband-headless.py"
    $PyArgs = @("python3", $HeadlessScript, "--bind", $BindAddress, "--player", $PlayerName)
    if ($NewGame) {
        $PyArgs += "--new-game"
    }
    $WslLaunchArgs = $WslPrefixArgs + @("--") + $PyArgs

    Write-Host "Starting headless Angband host in the background (WSL) on ${BindAddress}:47652..."
    Write-Host "Log: $HostLog"
    $HostProc = Start-Process -FilePath $WslCmd.Source `
        -ArgumentList $WslLaunchArgs `
        -WindowStyle Hidden `
        -RedirectStandardOutput $HostLog `
        -RedirectStandardError $HostErrLog `
        -PassThru

    Write-Host "Waiting for Angband host to build (first run only) and enter the dungeon..."
    $Deadline = (Get-Date).AddSeconds(180)
    $Ready = $false
    while ((Get-Date) -lt $Deadline) {
        if ($HostProc.HasExited) {
            Write-Warning "Background Angband host exited with code $($HostProc.ExitCode)."
            if (Test-Path $HostLog) { Get-Content $HostLog -Tail 30 }
            if (Test-Path $HostErrLog) { Get-Content $HostErrLog -Tail 30 }
            throw "Background Angband host failed to start. See $HostLog and $HostErrLog."
        }
        if ((Test-Path $HostLog) -and (Select-String -Path $HostLog -Pattern "\[abfp-host\] READY" -Quiet)) {
            $Ready = $true
            break
        }
        Start-Sleep -Milliseconds 500
    }

    if (-not $Ready) {
        Write-Warning "Timed out waiting for Angband readiness message; check $HostLog."
    } else {
        $ReadyLine = (Select-String -Path $HostLog -Pattern "\[abfp-host\] READY" | Select-Object -Last 1).Line
        Write-Host $ReadyLine -ForegroundColor Green
    }
}

if (-not $NoEditor) {
    Write-Host "Launching Unreal Editor: $ProjectPath"
    Start-Process -FilePath $EditorExe -ArgumentList @("`"$ProjectPath`"")
}

Write-Host ""
if ($InteractiveHost) {
    Write-Host "Next steps:"
    Write-Host "  1. Finish Angband character creation/load in the WSL terminal window."
    Write-Host "  2. Press Play in Unreal Editor -> HUD switches to ANGBAND LIVE."
} else {
    Write-Host "Angband is running silently in the background on ${BindAddress}:47652."
    Write-Host "Press Play in Unreal Editor -> HUD will immediately show ANGBAND LIVE."
    Write-Host "Run .\tools\stop-demo.ps1 when you want to stop the background Angband host."
}
