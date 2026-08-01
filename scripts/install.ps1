# AI Mozc IME - Post-install helper for Windows
# Use after MSI install, or standalone setup when developing locally.

param(
    [switch]$Uninstall,
    [switch]$InstallOllama,
    [switch]$PullModel,
    [switch]$Help,
    [string]$InstallPath = "$env:ProgramFiles\Mozc"
)

$ErrorActionPreference = "Stop"

$DefaultModel = "gemma3:1b"
$ConfigDir = Join-Path $env:LOCALAPPDATA "Google\Mozc"
$SetupScript = Join-Path $InstallPath "setup_ai_mozc.ps1"

function Show-Help {
    Write-Host @"
AI Mozc IME - Windows Setup Helper

Usage: .\install.ps1 [options]

This script complements MozcAI64.msi installation.
If you installed via MSI, config seeding is usually already done.

Options:
    -InstallPath <path>   Mozc install directory (default: $env:ProgramFiles\Mozc)
    -InstallOllama         Open Ollama download page / install if missing
    -PullModel             Pull default AI model after Ollama setup
    -Uninstall             Remove user AI config only (does not remove Mozc)
    -Help                  Show this help

Examples:
    .\install.ps1
    .\install.ps1 -PullModel
    .\install.ps1 -InstallOllama -PullModel
"@
}

function Test-Admin {
    $principal = New-Object Security.Principal.WindowsPrincipal(
        [Security.Principal.WindowsIdentity]::GetCurrent())
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Invoke-SetupScript {
    if (Test-Path $SetupScript) {
        $args = @("-InstallDir", $InstallPath)
        if ($PullModel) { $args += "-PullModel" }
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $SetupScript @args
        return
    }

    Write-Host "setup_ai_mozc.ps1 not found. Running fallback setup..."
    if (-not (Test-Path $ConfigDir)) {
        New-Item -ItemType Directory -Path $ConfigDir -Force | Out-Null
    }

    $configPath = Join-Path $ConfigDir "ai_config.json"
    if (-not (Test-Path $configPath)) {
        $defaultConfig = Join-Path $InstallPath "ai_config.default.json"
        if (Test-Path $defaultConfig) {
            Copy-Item $defaultConfig $configPath
        }
    }
}

function Install-OllamaIfRequested {
    if (-not $InstallOllama) { return }

    if (Get-Command ollama -ErrorAction SilentlyContinue) {
        Write-Host "Ollama is already installed."
        return
    }

    Write-Host "Please install Ollama from: https://ollama.ai/download/windows"
    Start-Process "https://ollama.ai/download/windows"
}

function Show-Status {
    Write-Host ""
    Write-Host "Config: $(Join-Path $ConfigDir 'ai_config.json')"
    Write-Host "Log:    $(Join-Path $ConfigDir 'ai_log.txt')"

    try {
        Invoke-WebRequest -Uri "http://localhost:11434/api/tags" -UseBasicParsing -TimeoutSec 2 | Out-Null
        Write-Host "Ollama: Running" -ForegroundColor Green
    } catch {
        Write-Host "Ollama: Not running (start with: ollama serve)" -ForegroundColor Yellow
    }
}

if ($Help) {
    Show-Help
    exit 0
}

if ($Uninstall) {
    $configPath = Join-Path $ConfigDir "ai_config.json"
    if (Test-Path $configPath) {
        Remove-Item $configPath -Force
        Write-Host "Removed: $configPath"
    }
    exit 0
}

Write-Host "AI Mozc IME - Windows Setup" -ForegroundColor Cyan
Invoke-SetupScript
Install-OllamaIfRequested
Show-Status
Write-Host ""
Write-Host "Setup complete." -ForegroundColor Green
