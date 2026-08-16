# Mozc AI v1.0 - reproducible all-in-one Windows MSI packager.

param(
    [string]$MozcDir = "",
    [string]$MozcRepo = "https://github.com/google/mozc.git",
    [string]$MozcRef = "3f235b4eb6fcff7d14ef5f0fb8ee56de7ee4c732",
    [string]$OutputDir = "",
    [switch]$SkipRuntime,
    [switch]$SkipDeps,
    [switch]$SkipQt,
    [switch]$DryRun,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

function Show-Help {
    Write-Host @"
Mozc AI v1.0 - Windows MSI Packager

Usage: .\package_windows.ps1 [options]

Options:
    -MozcDir <path>     Existing mozc/src directory (if omitted, clones to .mozc-build)
    -MozcRepo <url>     Mozc git repository URL
    -MozcRef <ref>       Pinned Mozc commit (default is the tested v1.0 base)
    -OutputDir <path>   Copy MSI here (default: dist\)
    -SkipRuntime         Reuse an already-built runtime\bundle
    -SkipDeps            Skip python build_tools/update_deps.py
    -SkipQt              Skip Qt build (only if already built)
    -DryRun              Show commands without executing integration/build
    -Help                Show this help

Prerequisites:
    - Visual Studio 2022 with C++ workload and Windows SDK
    - Python 3
    - Git
    - Bazelisk
    - .NET SDK (for WiX via Mozc)

Output:
    MozcAI-1.0.1-x64.msi (Mozc + local AI runtime + model)

Example:
    .\package_windows.ps1
    .\package_windows.ps1 -MozcDir C:\src\mozc\src -OutputDir .\dist
"@
}

function Require-Command {
    param([string]$Name)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required command not found: $Name"
    }
}

function Invoke-Step {
    param(
        [string]$Title,
        [scriptblock]$Action
    )
    Write-Host ""
    Write-Host "== $Title ==" -ForegroundColor Cyan
    if ($DryRun) {
        Write-Host "[dry-run] skipped"
        return
    }
    & $Action
    if ($LASTEXITCODE -ne 0) {
        throw "Step failed: $Title (exit $LASTEXITCODE)"
    }
}

if ($Help) {
    Show-Help
    exit 0
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
if (-not $OutputDir) {
    $OutputDir = Join-Path $ProjectRoot "dist"
}

Require-Command git
Require-Command python
Require-Command bazelisk

if (-not $SkipRuntime) {
    Invoke-Step "Build local-only AI runtime" {
        & (Join-Path $ScriptDir "build_runtime_bundle.ps1")
    }
}

if (-not $MozcDir) {
    $WorkRoot = Join-Path $ProjectRoot ".mozc-build"
    $CloneRoot = Join-Path $WorkRoot "mozc"
    $MozcDir = Join-Path $CloneRoot "src"

    Invoke-Step "Clone Mozc" {
        if (-not (Test-Path $CloneRoot)) {
            git clone $MozcRepo $CloneRoot
        }
        Push-Location $CloneRoot
        git fetch origin $MozcRef
        git checkout $MozcRef
        Pop-Location
    }
}

if (-not (Test-Path (Join-Path $MozcDir "MODULE.bazel"))) {
    throw "Invalid Mozc directory: $MozcDir"
}

Invoke-Step "Integrate AI module" {
    python (Join-Path $ScriptDir "integrate_mozc.py") --mozc-dir $MozcDir
}

Invoke-Step "Integrate Windows installer assets" {
    python (Join-Path $ScriptDir "integrate_mozc_installer.py") --mozc-dir $MozcDir
}

Push-Location $MozcDir
try {
    if (-not $SkipDeps) {
        Invoke-Step "Download Mozc build dependencies" {
            python build_tools/update_deps.py
        }
    }

    if (-not $SkipQt) {
        Invoke-Step "Build Qt dependencies" {
            python build_tools/build_qt.py --release --confirm_license
        }
    }

    Invoke-Step "Test integrated reranker" {
        bazelisk test //rewriter:rerank_rewriter_test --config release_build
    }

    Invoke-Step "Build Mozc AI v1.0 MSI" {
        bazelisk build package --config release_build
    }
}
finally {
    Pop-Location
}

$MsiPath = Join-Path $MozcDir "bazel-bin\win32\installer\MozcAI-1.0.1-x64.msi"
if (-not $DryRun) {
    if (-not (Test-Path $MsiPath)) {
        throw "MSI not found at expected path: $MsiPath"
    }

    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
    $Dest = Join-Path $OutputDir "MozcAI-1.0.1-x64.msi"
    Copy-Item -Path $MsiPath -Destination $Dest -Force

    Write-Host ""
    Write-Host "Package created:" -ForegroundColor Green
    Write-Host "  $Dest"
    Write-Host ""
    Write-Host "The MSI has its own Mozc AI product identity and migrates legacy Mozc."
    Write-Host "No conversion text logging is enabled by default."
}
