# AI Mozc - install MozcAI64.msi with forced file overwrite
#
# Windows Installer default (REINSTALLMODE=omus) will NOT replace an EXE
# when the PE FileVersion is unchanged. Our AI rebuilds keep Mozc's version
# resource, so reinstalling the MSI leaves the old mozc_server.exe in place.
#
# This script forces equal/older version overwrite (emus) and stops Mozc
# processes first so files are not locked.

param(
    [string]$MsiPath = "",
    [switch]$UninstallFirst,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

function Show-Help {
    Write-Host @"
Usage: .\scripts\install_msi.ps1 [-MsiPath path\to\MozcAI64.msi] [-UninstallFirst]

  -MsiPath         MSI to install (default: dist\MozcAI64.msi)
  -UninstallFirst  Remove existing Mozc product before install (most reliable)

Must run from an elevated (Administrator) PowerShell.
"@
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
$MsiPath = (Resolve-Path $MsiPath).Path

Write-Host "MSI: $MsiPath"
Write-Host "MSI time: $((Get-Item $MsiPath).LastWriteTime)"

Write-Host "Stopping Mozc processes..."
Get-Process mozc_server, mozc_tool, mozc_renderer -ErrorAction SilentlyContinue |
    Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1

if ($UninstallFirst) {
    Write-Host "Uninstalling existing Mozc (if present)..."
    $products = Get-CimInstance Win32_Product -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match 'Mozc|Google Japanese Input|Google 日本語入力' }
    foreach ($p in $products) {
        Write-Host "  Removing $($p.Name) ($($p.IdentifyingNumber))"
        $p | Invoke-CimMethod -MethodName Uninstall | Out-Null
    }
}

# emus = overwrite when missing / equal / older version (defeats same-FileVersion skip)
Write-Host "Installing with REINSTALLMODE=emus (force equal-version overwrite)..."
$log = Join-Path $env:TEMP "MozcAI_install.log"
$msiArgs = @(
    "/i", "`"$MsiPath`"",
    "REINSTALLMODE=emus",
    "/qb+",
    "/L*v", "`"$log`""
)
$proc = Start-Process -FilePath "msiexec.exe" -ArgumentList $msiArgs -Wait -PassThru
if ($proc.ExitCode -ne 0 -and $proc.ExitCode -ne 3010) {
    throw "msiexec failed with exit code $($proc.ExitCode). See $log"
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
Write-Host ""
Write-Host "Next: reboot, type in IME, then check:"
Write-Host "  explorer `"$env:USERPROFILE\AppData\LocalLow\Mozc`""
if ($proc.ExitCode -eq 3010) {
    Write-Host "Reboot required (msiexec 3010)." -ForegroundColor Yellow
}
