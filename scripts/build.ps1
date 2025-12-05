# Copyright 2024 AI Mozc IME Project
# Windows Build Script

param(
    [switch]$Release,
    [switch]$Clean,
    [switch]$Test,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

function Show-Help {
    Write-Host @"
AI Mozc IME Build Script

Usage: .\build.ps1 [options]

Options:
    -Release    Build in release mode (optimized)
    -Clean      Clean build directory before building
    -Test       Run unit tests after building
    -Help       Show this help message

Examples:
    .\build.ps1                 # Debug build
    .\build.ps1 -Release        # Release build
    .\build.ps1 -Clean -Test    # Clean build and run tests
"@
}

if ($Help) {
    Show-Help
    exit 0
}

# Check for Bazel
$bazel = Get-Command bazelisk -ErrorAction SilentlyContinue
if (-not $bazel) {
    $bazel = Get-Command bazel -ErrorAction SilentlyContinue
    if (-not $bazel) {
        Write-Error "Bazel or Bazelisk not found. Please install bazelisk: choco install bazelisk"
        exit 1
    }
}

Write-Host "========================================"
Write-Host "AI Mozc IME Build"
Write-Host "========================================"

# Set working directory
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir

Set-Location $projectRoot
Write-Host "Working directory: $projectRoot"

# Clean if requested
if ($Clean) {
    Write-Host "`nCleaning build directory..."
    & $bazel.Source clean
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Clean failed"
        exit 1
    }
}

# Build configuration
$config = if ($Release) { "release_build" } else { "debug" }
Write-Host "`nBuilding with config: $config"

# Build
Write-Host "`nBuilding AI module..."
& $bazel.Source build --config=windows --config=$config //src/ai:ai

if ($LASTEXITCODE -ne 0) {
    Write-Error "AI module build failed"
    exit 1
}

Write-Host "`nBuilding AIRewriter..."
& $bazel.Source build --config=windows --config=$config //src/rewriter:ai_rewriter

if ($LASTEXITCODE -ne 0) {
    Write-Error "AIRewriter build failed"
    exit 1
}

Write-Host "`n========================================"
Write-Host "Build successful!"
Write-Host "========================================"

# Run tests if requested
if ($Test) {
    Write-Host "`nRunning tests..."

    & $bazel.Source test --config=windows --config=$config //src/ai:all //src/rewriter:all

    if ($LASTEXITCODE -ne 0) {
        Write-Warning "Some tests failed"
    } else {
        Write-Host "All tests passed!"
    }
}

Write-Host "`nDone."
