# Copyright 2024 AI Mozc IME Project
# Windows Build Script

param(
    [switch]$Release,
    [switch]$Clean,
    [switch]$Test,
    [switch]$Help,
    [switch]$CheckOnly
)

# Don't stop on first error - we'll handle errors ourselves
$ErrorActionPreference = "Continue"

function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $color = switch ($Level) {
        "ERROR" { "Red" }
        "WARN"  { "Yellow" }
        "OK"    { "Green" }
        default { "White" }
    }
    Write-Host "[$timestamp] [$Level] $Message" -ForegroundColor $color
}

function Show-Help {
    Write-Host @"
AI Mozc IME Build Script

Usage: .\build.ps1 [options]

Options:
    -Release    Build in release mode (optimized)
    -Clean      Clean build directory before building
    -Test       Run unit tests after building
    -CheckOnly  Only check prerequisites, don't build
    -Help       Show this help message

Examples:
    .\build.ps1                 # Debug build
    .\build.ps1 -Release        # Release build
    .\build.ps1 -Clean -Test    # Clean build and run tests
    .\build.ps1 -CheckOnly      # Check if Bazel is installed

Prerequisites:
    1. Install Bazelisk: choco install bazelisk
       Or download from: https://github.com/bazelbuild/bazelisk/releases
    2. Install Visual Studio Build Tools (C++ workload)
    3. Ensure BAZEL_VC environment variable is set (optional)

Troubleshooting:
    - If you see 'fatal' errors, check that Bazel is properly installed
    - Run with -CheckOnly to verify prerequisites
    - Check docs/GETTING_STARTED.md for detailed setup instructions
"@
}

function Test-Prerequisites {
    Write-Log "Checking prerequisites..."
    $allOk = $true

    # Check for Bazel/Bazelisk
    Write-Host ""
    Write-Log "Checking for Bazelisk/Bazel..."

    $bazelisk = Get-Command bazelisk -ErrorAction SilentlyContinue
    $bazel = Get-Command bazel -ErrorAction SilentlyContinue

    if ($bazelisk) {
        Write-Log "Found Bazelisk: $($bazelisk.Source)" "OK"
        return $bazelisk.Source
    }
    elseif ($bazel) {
        Write-Log "Found Bazel: $($bazel.Source)" "OK"
        return $bazel.Source
    }
    else {
        Write-Log "Bazel/Bazelisk not found!" "ERROR"
        Write-Host ""
        Write-Host "To install Bazelisk:" -ForegroundColor Yellow
        Write-Host "  Option 1: choco install bazelisk" -ForegroundColor Cyan
        Write-Host "  Option 2: Download from https://github.com/bazelbuild/bazelisk/releases" -ForegroundColor Cyan
        Write-Host ""
        return $null
    }
}

function Find-VisualCpp {
    # Find Visual Studio installation using vswhere
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

    if (Test-Path $vswhere) {
        # Find VS installation with C++ workload
        $vsPath = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null

        if ($vsPath) {
            $vcPath = Join-Path $vsPath "VC"
            if (Test-Path $vcPath) {
                Write-Log "Found Visual C++: $vcPath" "OK"
                return @{
                    VsPath = $vsPath
                    VcPath = $vcPath
                }
            }
        }

        # Fallback: find any VS installation
        $vsPath = & $vswhere -latest -property installationPath 2>$null
        if ($vsPath) {
            $vcPath = Join-Path $vsPath "VC"
            if (Test-Path $vcPath) {
                Write-Log "Found Visual Studio: $vsPath" "OK"
                Write-Log "Note: C++ workload might not be fully installed" "WARN"
                return @{
                    VsPath = $vsPath
                    VcPath = $vcPath
                }
            }
        }
    }

    # Check for standalone Build Tools
    $buildToolsPaths = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\BuildTools",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community"
    )

    foreach ($path in $buildToolsPaths) {
        $vcPath = Join-Path $path "VC"
        if (Test-Path $vcPath) {
            Write-Log "Found Visual C++: $vcPath" "OK"
            return @{
                VsPath = $path
                VcPath = $vcPath
            }
        }
    }

    Write-Log "Visual C++ Build Tools not found" "ERROR"
    Write-Host ""
    Write-Host "Please install Visual Studio with C++ workload:" -ForegroundColor Yellow
    Write-Host "  1. Download Visual Studio Installer" -ForegroundColor Cyan
    Write-Host "  2. Select 'Desktop development with C++' workload" -ForegroundColor Cyan
    Write-Host "  3. Ensure 'MSVC v143' and 'Windows SDK' are checked" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Or install standalone Build Tools:" -ForegroundColor Yellow
    Write-Host "  https://visualstudio.microsoft.com/visual-cpp-build-tools/" -ForegroundColor Cyan
    Write-Host ""
    return $null
}

function Set-BazelEnvironment {
    param($VcInfo)

    if (-not $VcInfo) {
        return $false
    }

    # Set BAZEL_VC environment variable for this session
    $env:BAZEL_VC = $VcInfo.VcPath
    Write-Log "Set BAZEL_VC=$($VcInfo.VcPath)"

    # Try to find and set BAZEL_VC_FULL_VERSION
    $toolsVersionDir = Join-Path $VcInfo.VcPath "Tools\MSVC"
    if (Test-Path $toolsVersionDir) {
        $versions = Get-ChildItem -Path $toolsVersionDir -Directory | Sort-Object Name -Descending
        if ($versions.Count -gt 0) {
            $latestVersion = $versions[0].Name
            $env:BAZEL_VC_FULL_VERSION = $latestVersion
            Write-Log "Set BAZEL_VC_FULL_VERSION=$latestVersion"
        }
    }

    return $true
}

# Main script starts here
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "AI Mozc IME Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

if ($Help) {
    Show-Help
    exit 0
}

# Check prerequisites
$bazelPath = Test-Prerequisites

if (-not $bazelPath) {
    Write-Host ""
    Write-Log "Build cannot continue without Bazel." "ERROR"
    Write-Host ""
    Write-Host "Press any key to exit..." -ForegroundColor Gray
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    exit 1
}

# Find and configure Visual C++
Write-Log "Checking for Visual C++..."
$vcInfo = Find-VisualCpp

if (-not $vcInfo) {
    Write-Host ""
    Write-Log "Build cannot continue without Visual C++." "ERROR"
    Write-Host ""
    Write-Host "Press any key to exit..." -ForegroundColor Gray
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    exit 1
}

# Set Bazel environment variables
Set-BazelEnvironment -VcInfo $vcInfo | Out-Null

if ($CheckOnly) {
    Write-Host ""
    Write-Log "Prerequisites check complete." "OK"
    Write-Host ""
    Write-Host "Environment variables set for this session:" -ForegroundColor Gray
    Write-Host "  BAZEL_VC = $env:BAZEL_VC" -ForegroundColor Gray
    if ($env:BAZEL_VC_FULL_VERSION) {
        Write-Host "  BAZEL_VC_FULL_VERSION = $env:BAZEL_VC_FULL_VERSION" -ForegroundColor Gray
    }
    Write-Host ""
    exit 0
}

# Set working directory
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir

try {
    Set-Location $projectRoot
    Write-Log "Working directory: $projectRoot"
}
catch {
    Write-Log "Failed to change to project directory: $_" "ERROR"
    exit 1
}

# Clean if requested
if ($Clean) {
    Write-Host ""
    Write-Log "Cleaning build directory..."
    try {
        & $bazelPath clean 2>&1 | ForEach-Object { Write-Host "  $_" }
        if ($LASTEXITCODE -ne 0) {
            Write-Log "Clean returned non-zero exit code, but continuing..." "WARN"
        }
    }
    catch {
        Write-Log "Clean failed: $_" "WARN"
    }
}

# Build configuration
$config = if ($Release) { "release_build" } else { "debug" }
Write-Host ""
Write-Log "Building with config: $config"

# Build AI module
Write-Host ""
Write-Log "Building AI module..."
try {
    & $bazelPath build --config=$config //src/ai:ai 2>&1 | ForEach-Object {
        if ($_ -match "ERROR|error:|fatal") {
            Write-Host "  $_" -ForegroundColor Red
        } elseif ($_ -match "WARNING|warning:") {
            Write-Host "  $_" -ForegroundColor Yellow
        } else {
            Write-Host "  $_"
        }
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Log "AI module build failed with exit code: $LASTEXITCODE" "ERROR"
        Write-Host ""
        Write-Host "Common issues:" -ForegroundColor Yellow
        Write-Host "  - Missing Visual Studio Build Tools" -ForegroundColor Gray
        Write-Host "  - C++ workload not installed in Visual Studio" -ForegroundColor Gray
        Write-Host "  - Run: Visual Studio Installer > Modify > 'Desktop development with C++'" -ForegroundColor Gray
        Write-Host ""
        Write-Host "Current BAZEL_VC: $env:BAZEL_VC" -ForegroundColor Gray
        Write-Host ""
        Write-Host "Check docs/GETTING_STARTED.md for troubleshooting" -ForegroundColor Cyan
        exit 1
    }
}
catch {
    Write-Log "Build error: $_" "ERROR"
    exit 1
}

# Build AIRewriter
Write-Host ""
Write-Log "Building AIRewriter..."
try {
    & $bazelPath build --config=$config //src/rewriter:ai_rewriter 2>&1 | ForEach-Object {
        if ($_ -match "ERROR|error:|fatal") {
            Write-Host "  $_" -ForegroundColor Red
        } elseif ($_ -match "WARNING|warning:") {
            Write-Host "  $_" -ForegroundColor Yellow
        } else {
            Write-Host "  $_"
        }
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Log "AIRewriter build failed with exit code: $LASTEXITCODE" "ERROR"
        exit 1
    }
}
catch {
    Write-Log "Build error: $_" "ERROR"
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Log "Build successful!" "OK"
Write-Host "========================================" -ForegroundColor Green

# Run tests if requested
if ($Test) {
    Write-Host ""
    Write-Log "Running tests..."

    try {
        & $bazelPath test --config=$config //src/ai:all //src/rewriter:all 2>&1 | ForEach-Object {
            if ($_ -match "PASSED") {
                Write-Host "  $_" -ForegroundColor Green
            } elseif ($_ -match "FAILED") {
                Write-Host "  $_" -ForegroundColor Red
            } else {
                Write-Host "  $_"
            }
        }

        if ($LASTEXITCODE -ne 0) {
            Write-Log "Some tests failed" "WARN"
        } else {
            Write-Log "All tests passed!" "OK"
        }
    }
    catch {
        Write-Log "Test error: $_" "ERROR"
    }
}

Write-Host ""
Write-Log "Done."
Write-Host ""
