# AI Mozc IME - pre-install cleanup
# Stops stale Mozc processes and removes legacy x86 install paths.
# Runs automatically from MSI; safe to run manually before reinstall.

param(
    [switch]$Quiet
)

$ErrorActionPreference = "Continue"

function Write-CleanupLog {
    param([string]$Message)
    if (-not $Quiet) {
        Write-Host "[AI-Mozc Cleanup] $Message"
    }
}

$processNames = @("mozc_server", "mozc_tool", "mozc_renderer")

foreach ($name in $processNames) {
    $procs = Get-Process -Name $name -ErrorAction SilentlyContinue
    foreach ($proc in $procs) {
        try {
            Write-CleanupLog "Stopping process: $($proc.ProcessName) (PID $($proc.Id))"
            Stop-Process -Id $proc.Id -Force -ErrorAction Stop
        } catch {
            Write-CleanupLog "Could not stop $($proc.ProcessName): $_"
        }
    }
}

Start-Sleep -Milliseconds 500

$legacyPaths = @()
if (${env:ProgramFiles(x86)}) {
    $legacyPaths += (Join-Path ${env:ProgramFiles(x86)} "Mozc")
}

foreach ($path in $legacyPaths) {
    if (Test-Path $path) {
        Write-CleanupLog "Removing legacy install path: $path"
        try {
            Remove-Item -Path $path -Recurse -Force -ErrorAction Stop
        } catch {
            Write-CleanupLog "Could not remove $path (may be in use): $_"
        }
    }
}

$configDir = Join-Path $env:LOCALAPPDATA "Google\Mozc"
$staleLog = Join-Path $configDir "ai_log.txt"
if (Test-Path $staleLog) {
    Write-CleanupLog "Removing stale log: $staleLog"
    try {
        Remove-Item -Path $staleLog -Force -ErrorAction Stop
    } catch {
        Write-CleanupLog "Could not remove log file: $_"
    }
}

Write-CleanupLog "Pre-install cleanup complete"
exit 0
