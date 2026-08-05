# AI Mozc - install MozcAI64.msi with forced file overwrite + diagnostics
#
# Windows Installer default (REINSTALLMODE=omus) will NOT replace an EXE
# when the PE FileVersion is unchanged. Use REINSTALLMODE=emus.
#
# Also: UI /qb+ closes the error dialog almost immediately. This script
# keeps a full UI and always writes/parses a verbose MSI log.

param(
    [string]$MsiPath = "",
    [switch]$UninstallFirst,
    [switch]$DiagOnly,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

function Show-Help {
    Write-Host @"
Usage: .\scripts\install_msi.ps1 [options]

  -MsiPath         MSI to install (default: dist\MozcAI64.msi)
  -UninstallFirst  Remove registered Mozc products before install
  -DiagOnly        Print environment / residual product info and exit

Must run from an elevated (Administrator) PowerShell.
"@
}

function Get-MozcResiduals {
    $paths = @(
        "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*",
        "HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*"
    )
    $hits = @()
    foreach ($p in $paths) {
        $hits += Get-ItemProperty $p -ErrorAction SilentlyContinue |
            Where-Object {
                $_.DisplayName -match 'Mozc|Google Japanese Input|Google 日本語入力' -or
                $_.Publisher -match 'Google' -and $_.DisplayName -match 'Mozc|Japanese'
            }
    }
    return $hits | Sort-Object DisplayName -Unique
}

function Show-MsiLogErrors {
    param([string]$LogPath)
    if (-not (Test-Path $LogPath)) {
        Write-Host "No MSI log at $LogPath"
        return
    }
    Write-Host ""
    Write-Host "=== MSI log errors (from $LogPath) ===" -ForegroundColor Yellow
    Select-String -Path $LogPath -Pattern @(
        'Return value 3',
        'Error [0-9]{4}',
        '2762',
        '1603',
        '1619',
        'NEWERVERSIONDETECTED',
        'MainEngineThread is returning',
        'Installation success or error status'
    ) |
        Select-Object -Last 40 |
        ForEach-Object { $_.Line }
}

if ($Help) {
    Show-Help
    exit 0
}

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).
    IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    throw "Run this script as Administrator."
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
if (-not $MsiPath) {
    $MsiPath = Join-Path $ProjectRoot "dist\MozcAI64.msi"
}
if (-not (Test-Path $MsiPath)) {
    throw "MSI not found: $MsiPath — build with .\scripts\package_windows.ps1 first"
}
$MsiPath = (Resolve-Path $MsiPath).Path
$msiItem = Get-Item $MsiPath

Write-Host "MSI:  $MsiPath"
Write-Host "MSI time: $($msiItem.LastWriteTime)  size=$($msiItem.Length)"
Write-Host ""

# Hint if tree still has the bad deferred-before-InstallValidate sequencing
$wxs = Join-Path $ProjectRoot ".mozc-build\mozc\src\win32\installer\installer_oss_64bit.wxs"
if (Test-Path $wxs) {
    $wxsText = Get-Content $wxs -Raw
    if ($wxsText -match 'PreInstallCleanup" Before="InstallValidate"' -and
        $wxsText -match 'Execute="deferred"') {
        Write-Host "WARNING: .mozc-build WiX still has deferred PreInstallCleanup before InstallValidate." -ForegroundColor Red
        Write-Host "         That MSI will fail with error 2762. Rebuild after:" -ForegroundColor Red
        Write-Host "         git pull  (base branch with PR #12/#13)" -ForegroundColor Red
        Write-Host "         .\scripts\package_windows.ps1 -SkipDeps -SkipQt -MozcRef 3f235b4eb6fcff7d14ef5f0fb8ee56de7ee4c732" -ForegroundColor Red
    } elseif ($wxsText -match 'SeedAIConfig" After="InstallFinalize"') {
        Write-Host "WiX check: SeedAIConfig After InstallFinalize (2762-safe) OK" -ForegroundColor Green
    }
}

Write-Host "Residual uninstall entries:"
$residuals = Get-MozcResiduals
if ($residuals) {
    $residuals | ForEach-Object {
        Write-Host ("  - {0}  ver={1}  id={2}" -f $_.DisplayName, $_.DisplayVersion, $_.PSChildName)
    }
} else {
    Write-Host "  (none)"
}

$serverNow = Join-Path $env:ProgramFiles "Mozc\mozc_server.exe"
Write-Host "Program Files server: $(if (Test-Path $serverNow) { (Get-Item $serverNow).LastWriteTime } else { 'missing' })"

if ($DiagOnly) {
    exit 0
}

Write-Host ""
Write-Host "Stopping Mozc processes..."
Get-Process mozc_server, mozc_tool, mozc_renderer -ErrorAction SilentlyContinue |
    Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1

if ($UninstallFirst -or $residuals) {
    if (-not $UninstallFirst -and $residuals) {
        Write-Host "Residuals found — uninstalling them before install..." -ForegroundColor Yellow
    } else {
        Write-Host "Uninstalling existing Mozc products..."
    }
    foreach ($r in (Get-MozcResiduals)) {
        $guid = $r.PSChildName
        if ($guid -match '^\{[0-9A-Fa-f-]+\}$') {
            Write-Host "  msiexec /x $guid"
            $u = Start-Process msiexec.exe -ArgumentList @("/x", $guid, "/qb!") -Wait -PassThru
            Write-Host "    exit $($u.ExitCode)"
        }
    }
    Start-Sleep -Seconds 2
}

$log = Join-Path $env:TEMP "MozcAI_install.log"
if (Test-Path $log) { Remove-Item $log -Force }

# Full UI (no /qb+) so error dialogs stay visible. emus forces equal-version overwrite.
Write-Host "Installing (full UI, REINSTALLMODE=emus)..."
Write-Host "Log: $log"
$msiArgs = @(
    "/i", "`"$MsiPath`"",
    "REINSTALLMODE=emus",
    "/L*v", "`"$log`""
)
$proc = Start-Process -FilePath "msiexec.exe" -ArgumentList $msiArgs -Wait -PassThru
Write-Host "msiexec exit: $($proc.ExitCode)"

Show-MsiLogErrors -LogPath $log

if ($proc.ExitCode -ne 0 -and $proc.ExitCode -ne 3010) {
    Write-Host ""
    Write-Host "Install FAILED. Open the log and search for 'Return value 3' / '2762':" -ForegroundColor Red
    Write-Host "  notepad `"$log`""
    Write-Host ""
    Write-Host "If log shows 2762: rebuild MSI from latest base (PR #12 fix), then retry."
    Write-Host "If log shows NEWERVERSIONDETECTED: remove leftover Mozc from Apps & Features / registry."
    exit $proc.ExitCode
}

$server = Join-Path $env:ProgramFiles "Mozc\mozc_server.exe"
if (-not (Test-Path $server)) {
    throw "mozc_server.exe missing after install: $server"
}

$item = Get-Item $server
Write-Host ""
Write-Host "Installed:" -ForegroundColor Green
Write-Host "  $server"
Write-Host "  LastWriteTime: $($item.LastWriteTime)"
Write-Host "  Length: $($item.Length)"
if ($item.LastWriteTime -lt $msiItem.LastWriteTime.AddHours(-1)) {
    Write-Host "WARNING: server timestamp older than MSI — overwrite may have failed." -ForegroundColor Yellow
}
Write-Host ""
Write-Host "Next: reboot, type in IME, then:"
Write-Host "  explorer `"$env:USERPROFILE\AppData\LocalLow\Mozc`""
if ($proc.ExitCode -eq 3010) {
    Write-Host "Reboot required (msiexec 3010)." -ForegroundColor Yellow
}
