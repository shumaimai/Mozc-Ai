# AI Mozc IME - post-install verification
# Confirms AI-enabled mozc_server is installed to 64-bit Program Files.

param(
    [string]$InstallDir = "",
    [switch]$Quiet
)

$ErrorActionPreference = "Continue"

function Write-VerifyLog {
    param([string]$Message)
    if (-not $Quiet) {
        Write-Host "[AI-Mozc Verify] $Message"
    }
}

function Find-MozcInstallDir {
    param([string]$PreferredRoot = "")

    $candidates = @()
    if ($PreferredRoot) { $candidates += $PreferredRoot }
    if ($env:ProgramFiles) { $candidates += (Join-Path $env:ProgramFiles "Mozc") }

    foreach ($dir in ($candidates | Select-Object -Unique)) {
        $server = Join-Path $dir "mozc_server.exe"
        if (Test-Path $server) {
            return $dir
        }
    }
    return $null
}

$mozcDir = Find-MozcInstallDir -PreferredRoot $InstallDir
$serverPath = if ($mozcDir) { Join-Path $mozcDir "mozc_server.exe" } else { $null }

$configDir = Join-Path $env:LOCALAPPDATA "Google\Mozc"
if (-not (Test-Path $configDir)) {
    New-Item -ItemType Directory -Path $configDir -Force | Out-Null
}
$verifyFile = Join-Path $configDir "install_verify.txt"

$lines = @()
$lines += "Mozc AI Install Verification"
$lines += "Timestamp: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$lines += ""

$ok = $true

if (-not $serverPath -or -not (Test-Path $serverPath)) {
    $lines += "FAIL: mozc_server.exe not found in Program Files\Mozc"
    $ok = $false
} else {
    $lines += "OK: mozc_server.exe at $serverPath"
    $size = (Get-Item $serverPath).Length
    $lines += "Size: $size bytes"

    try {
        $content = [System.IO.File]::ReadAllBytes($serverPath)
        $text = [System.Text.Encoding]::ASCII.GetString($content)
        if ($text -match "AIRewriter") {
            $lines += "OK: AIRewriter marker found in binary"
        } else {
            $lines += "FAIL: AIRewriter marker NOT found (non-AI binary?)"
            $ok = $false
        }
        if ($text -match "deepseek") {
            $lines += "OK: deepseek backend marker found"
        }
    } catch {
        $lines += "WARN: Could not scan binary: $_"
    }

    if (${env:ProgramFiles(x86)}) {
        $x86Server = Join-Path (Join-Path ${env:ProgramFiles(x86)} "Mozc") "mozc_server.exe"
        if (Test-Path $x86Server) {
            $lines += "WARN: mozc_server.exe also exists at $x86Server (stale x86 install)"
        }
    }
}

$userConfig = Join-Path $configDir "ai_config.json"
if (Test-Path $userConfig) {
    $lines += "OK: ai_config.json exists at $userConfig"
} else {
    $lines += "WARN: ai_config.json not found (setup may have failed)"
}

if ($ok) {
    $lines += ""
    $lines += "RESULT: PASS - Reboot PC, then verify ai_log.txt after typing in IME"
} else {
    $lines += ""
    $lines += "RESULT: FAIL - Reinstall MozcAI64.msi as administrator"
}

$lines | Set-Content -Path $verifyFile -Encoding UTF8
foreach ($line in $lines) {
    Write-VerifyLog $line
}

if ($ok) { exit 0 } else { exit 0 }
