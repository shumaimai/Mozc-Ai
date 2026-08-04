# AI Mozc IME - fix stale x86 TIP registry path
# Mozc reads install dir from CLSID InprocServer32; must point to 64-bit path.

param(
    [switch]$Quiet
)

$ErrorActionPreference = "Continue"

function Write-FixLog {
    param([string]$Message)
    if (-not $Quiet) {
        Write-Host "[AI-Mozc Registry] $Message"
    }
}

$tipClsid = "{10A67BC8-22FA-4A59-90DC-2546652C56BF}"
$regPath = "HKLM:\SOFTWARE\Classes\CLSID\$tipClsid\InprocServer32"

$targetDll = $null
if ($env:ProgramFiles) {
    $candidate = Join-Path (Join-Path $env:ProgramFiles "Mozc") "mozc_tip64.dll"
    if (Test-Path $candidate) {
        $targetDll = $candidate
    }
}

if (-not $targetDll) {
    Write-FixLog "mozc_tip64.dll not found under Program Files\Mozc"
    exit 1
}

$current = $null
try {
    $current = (Get-ItemProperty -Path $regPath -ErrorAction Stop).'(default)'
} catch {
    Write-FixLog "Registry key not found: $regPath"
    exit 1
}

if ($current -ieq $targetDll) {
    Write-FixLog "Registry already correct: $targetDll"
    exit 0
}

Write-FixLog "Updating registry:"
Write-FixLog "  from: $current"
Write-FixLog "  to:   $targetDll"

try {
    Set-ItemProperty -Path $regPath -Name "(default)" -Value $targetDll
    Write-FixLog "Registry updated successfully"
    exit 0
} catch {
    Write-FixLog "Failed to update registry (run as Administrator): $_"
    exit 1
}
