[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$WslDistro = ""
)

$ErrorActionPreference = "Stop"

$WslCmd = Get-Command wsl.exe -ErrorAction SilentlyContinue
if (-not $WslCmd) {
    throw "wsl.exe was not found."
}

$WslPrefixArgs = @()
if ($WslDistro) {
    $WslPrefixArgs += @("-d", $WslDistro)
}

Write-Host "Saving and stopping any background Angband ABFP host in WSL..."
& wsl.exe @WslPrefixArgs -- bash -c '
if pgrep -f "run-angband-headless.py" >/dev/null 2>&1; then
    pkill -TERM -f "run-angband-headless.py" 2>/dev/null || true
    for _ in 1 2 3 4 5 6 7 8; do
        pgrep -f "game/angband" >/dev/null 2>&1 || break
        sleep 0.25
    done
fi
pkill -f "game/angband" 2>/dev/null || true
'
Write-Host "Done."
