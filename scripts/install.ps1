# AI Mozc IME Installer Script for Windows
# This script installs AI Mozc IME and sets up the required components

param(
    [switch]$Uninstall,
    [switch]$InstallOllama,
    [switch]$Help,
    [string]$InstallPath = "$env:ProgramFiles\Google\Mozc AI"
)

$ErrorActionPreference = "Stop"

# ==================== Configuration ====================
$MozcVersion = "2.29.5160.102"
$OllamaDownloadUrl = "https://ollama.ai/download/windows"
$DefaultModel = "mistral:7b"
$ConfigDir = "$env:LOCALAPPDATA\Google\Mozc"

# ==================== Helper Functions ====================

function Write-Header {
    param([string]$Message)
    Write-Host ""
    Write-Host "=" * 60 -ForegroundColor Cyan
    Write-Host " $Message" -ForegroundColor Cyan
    Write-Host "=" * 60 -ForegroundColor Cyan
    Write-Host ""
}

function Write-Step {
    param([string]$Message)
    Write-Host "[*] $Message" -ForegroundColor Yellow
}

function Write-Success {
    param([string]$Message)
    Write-Host "[OK] $Message" -ForegroundColor Green
}

function Write-Error {
    param([string]$Message)
    Write-Host "[ERROR] $Message" -ForegroundColor Red
}

function Test-Admin {
    $currentUser = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    return $currentUser.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Show-Help {
    Write-Host @"
AI Mozc IME Installer

Usage: .\install.ps1 [options]

Options:
    -InstallPath <path>   Installation directory (default: $env:ProgramFiles\Google\Mozc AI)
    -InstallOllama        Also install Ollama (AI backend)
    -Uninstall            Uninstall AI Mozc IME
    -Help                 Show this help message

Examples:
    .\install.ps1                           # Standard installation
    .\install.ps1 -InstallOllama            # Install with Ollama
    .\install.ps1 -Uninstall                # Uninstall
    .\install.ps1 -InstallPath "D:\Mozc"    # Custom install path

Note: This script requires Administrator privileges.
"@
}

# ==================== Installation Functions ====================

function Install-Prerequisites {
    Write-Header "Checking Prerequisites"

    # Check for Visual C++ Redistributable
    Write-Step "Checking Visual C++ Redistributable..."
    $vcRedist = Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" -ErrorAction SilentlyContinue
    if (-not $vcRedist) {
        Write-Host "  Visual C++ Redistributable not found. You may need to install it from:"
        Write-Host "  https://aka.ms/vs/17/release/vc_redist.x64.exe"
    } else {
        Write-Success "Visual C++ Redistributable found"
    }
}

function Install-Ollama {
    Write-Header "Installing Ollama"

    # Check if Ollama is already installed
    $ollamaPath = Get-Command ollama -ErrorAction SilentlyContinue
    if ($ollamaPath) {
        Write-Success "Ollama is already installed at: $($ollamaPath.Source)"
        return
    }

    Write-Step "Downloading Ollama installer..."
    $installerPath = "$env:TEMP\OllamaSetup.exe"

    try {
        Invoke-WebRequest -Uri $OllamaDownloadUrl -OutFile $installerPath -UseBasicParsing
        Write-Success "Downloaded Ollama installer"

        Write-Step "Running Ollama installer..."
        Start-Process -FilePath $installerPath -Wait
        Write-Success "Ollama installed"

        # Download default model
        Write-Step "Downloading AI model ($DefaultModel)..."
        & ollama pull $DefaultModel
        Write-Success "Model downloaded"
    }
    catch {
        Write-Error "Failed to install Ollama: $_"
        Write-Host "Please install Ollama manually from: $OllamaDownloadUrl"
    }
    finally {
        if (Test-Path $installerPath) {
            Remove-Item $installerPath -Force
        }
    }
}

function Install-MozcAI {
    Write-Header "Installing AI Mozc IME"

    # Create installation directory
    Write-Step "Creating installation directory..."
    if (-not (Test-Path $InstallPath)) {
        New-Item -ItemType Directory -Path $InstallPath -Force | Out-Null
    }
    Write-Success "Created: $InstallPath"

    # Copy files
    Write-Step "Copying files..."
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    $projectRoot = Split-Path -Parent $scriptDir

    # Copy built binaries (if they exist)
    $bazelBin = "$projectRoot\bazel-bin"
    if (Test-Path $bazelBin) {
        Copy-Item -Path "$bazelBin\*" -Destination $InstallPath -Recurse -Force -ErrorAction SilentlyContinue
        Write-Success "Copied built binaries"
    } else {
        Write-Host "  Note: No built binaries found. Run build.ps1 first."
    }

    # Create config directory
    Write-Step "Creating config directory..."
    if (-not (Test-Path $ConfigDir)) {
        New-Item -ItemType Directory -Path $ConfigDir -Force | Out-Null
    }
    Write-Success "Created: $ConfigDir"

    # Create default config file
    Write-Step "Creating default configuration..."
    $configPath = "$ConfigDir\ai_config.json"
    if (-not (Test-Path $configPath)) {
        $defaultConfig = @{
            enabled = $true
            backend_type = "ollama"
            ollama_endpoint = "http://localhost:11434"
            ollama_model = $DefaultModel
            connect_timeout_ms = 50
            request_timeout_ms = 500
            max_wait_ms = 600
            cache_ttl_seconds = 60
            cache_max_entries = 100
            history_size = 5
            log_level = "info"
            log_ai_communication = $false
            disable_ai = $false
            use_mock = $false
        } | ConvertTo-Json -Depth 10

        Set-Content -Path $configPath -Value $defaultConfig -Encoding UTF8
        Write-Success "Created config at: $configPath"
    } else {
        Write-Host "  Config already exists, skipping..."
    }
}

function Register-IME {
    Write-Header "Registering IME"

    Write-Step "Registering AI Mozc IME..."

    # Note: Actual IME registration requires more complex steps
    # This is a placeholder for the registration process
    Write-Host "  IME registration requires manual steps:"
    Write-Host "  1. Open Settings > Time & Language > Language"
    Write-Host "  2. Add Japanese language if not present"
    Write-Host "  3. Click on Japanese > Language options"
    Write-Host "  4. Add AI Mozc IME keyboard"
    Write-Host ""
    Write-Host "  For automatic registration, the IME DLL must be installed"
    Write-Host "  in the system directory and registered with the IME framework."
}

function Uninstall-MozcAI {
    Write-Header "Uninstalling AI Mozc IME"

    # Remove installation directory
    if (Test-Path $InstallPath) {
        Write-Step "Removing installation directory..."
        Remove-Item -Path $InstallPath -Recurse -Force
        Write-Success "Removed: $InstallPath"
    }

    # Ask about config
    if (Test-Path $ConfigDir) {
        $response = Read-Host "Remove configuration directory ($ConfigDir)? [y/N]"
        if ($response -eq "y" -or $response -eq "Y") {
            Remove-Item -Path $ConfigDir -Recurse -Force
            Write-Success "Removed: $ConfigDir"
        }
    }

    Write-Success "Uninstallation complete"
}

function Show-Summary {
    Write-Header "Installation Summary"

    Write-Host "AI Mozc IME has been installed!"
    Write-Host ""
    Write-Host "Installation Path: $InstallPath"
    Write-Host "Config Directory:  $ConfigDir"
    Write-Host "Config File:       $ConfigDir\ai_config.json"
    Write-Host "Log File:          $ConfigDir\ai_log.txt"
    Write-Host ""

    # Check Ollama status
    $ollamaRunning = $false
    try {
        $response = Invoke-WebRequest -Uri "http://localhost:11434/api/tags" -UseBasicParsing -TimeoutSec 2
        $ollamaRunning = $true
    } catch {}

    if ($ollamaRunning) {
        Write-Host "Ollama Status: " -NoNewline
        Write-Host "Running" -ForegroundColor Green
    } else {
        Write-Host "Ollama Status: " -NoNewline
        Write-Host "Not Running" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "To start Ollama, run: ollama serve"
    }

    Write-Host ""
    Write-Host "Next Steps:"
    Write-Host "1. Ensure Ollama is running: ollama serve"
    Write-Host "2. Download a model: ollama pull $DefaultModel"
    Write-Host "3. Register the IME in Windows Settings"
    Write-Host ""
}

# ==================== Main ====================

if ($Help) {
    Show-Help
    exit 0
}

Write-Host @"

    _    ___   __  __                   ___ __  __ _____
   / \  |_ _| |  \/  | ___ ______ __   |_ _|  \/  | ____|
  / _ \  | |  | |\/| |/ _ \_  /  ___|   | || |\/| |  _|
 / ___ \ | |  | |  | | (_) / /| (___    | || |  | | |___
/_/   \_\___| |_|  |_|\___/___|\___|   |___|_|  |_|_____|

"@ -ForegroundColor Cyan

if (-not (Test-Admin)) {
    Write-Error "This script requires Administrator privileges."
    Write-Host "Please run PowerShell as Administrator and try again."
    exit 1
}

if ($Uninstall) {
    Uninstall-MozcAI
    exit 0
}

# Normal installation
Install-Prerequisites

if ($InstallOllama) {
    Install-Ollama
}

Install-MozcAI
Register-IME
Show-Summary

Write-Host "Installation complete!" -ForegroundColor Green
