# Copyright 2024 AI Mozc IME Project
# Mozc Integration Script for Windows

param(
    [Parameter(Mandatory = $true)]
    [string]$MozcDir,

    [switch]$DryRun,
    [switch]$Help
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

if ($Help) {
    Write-Host @"
AI Mozc IME - Mozc Integration Script

Usage: .\integrate_mozc.ps1 -MozcDir <path> [options]

Required:
    -MozcDir <path>    Path to Mozc source directory (mozc/src)

Options:
    -DryRun            Show what would be done without making changes
    -Help              Show this help message

Example:
    .\integrate_mozc.ps1 -MozcDir C:\mozc\src
    .\integrate_mozc.ps1 -MozcDir C:\mozc\src -DryRun
"@
    exit 0
}

$ArgsList = @("--mozc-dir", $MozcDir)
if ($DryRun) {
    $ArgsList += "--dry-run"
}

python "$ScriptDir\integrate_mozc.py" @ArgsList
