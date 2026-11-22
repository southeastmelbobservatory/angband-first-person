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

Write-Host "Stopping any background Angband ABFP host in WSL..."
& wsl.exe @WslPrefixArgs -- bash -c 'pkill -f "run-angband-headless.py" 2>/dev/null || true; pkill -f "game/angband" 2>/dev/null || true'
Write-Host "Done."
