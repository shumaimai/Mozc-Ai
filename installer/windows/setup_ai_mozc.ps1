# AI Mozc IME - post-install setup
# Seeds user config from the installed default template.

param(
    [string]$InstallDir = "",
    [switch]$PullModel,
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"

function Write-SetupLog {
    param([string]$Message)
    if (-not $Quiet) {
        Write-Host "[AI-Mozc Setup] $Message"
    }
}

function Resolve-MozcInstallDir {
    param([string]$Preferred)

    $candidates = @()
    if ($Preferred) {
        $candidates += $Preferred
    }
    if ($env:ProgramFiles) {
        $candidates += (Join-Path $env:ProgramFiles "Mozc")
    }
    if (${env:ProgramFiles(x86)}) {
        $candidates += (Join-Path ${env:ProgramFiles(x86)} "Mozc")
    }

    foreach ($dir in ($candidates | Select-Object -Unique)) {
        $defaultConfig = Join-Path $dir "ai_config.default.json"
        if (Test-Path $defaultConfig) {
            return $dir
        }
    }

    foreach ($dir in ($candidates | Select-Object -Unique)) {
        $setupMarker = Join-Path $dir "setup_ai_mozc.ps1"
        if (Test-Path $setupMarker) {
            return $dir
        }
    }

    if ($Preferred) { return $Preferred }
    if ($env:ProgramFiles) { return (Join-Path $env:ProgramFiles "Mozc") }
    return "C:\Program Files\Mozc"
}

$InstallDir = Resolve-MozcInstallDir -Preferred $InstallDir

$ConfigDir = Join-Path $env:LOCALAPPDATA "Google\Mozc"
$UserConfig = Join-Path $ConfigDir "ai_config.json"
$DefaultConfig = Join-Path $InstallDir "ai_config.default.json"

if (-not (Test-Path $DefaultConfig)) {
    Write-SetupLog "Default config not found: $DefaultConfig"
    Write-SetupLog "Checked install dir: $InstallDir"
    Write-SetupLog "Also try: Program Files\Mozc and Program Files (x86)\Mozc"
    exit 0
}

Write-SetupLog "Using Mozc install dir: $InstallDir"

if (-not (Test-Path $ConfigDir)) {
    New-Item -ItemType Directory -Path $ConfigDir -Force | Out-Null
    Write-SetupLog "Created config directory: $ConfigDir"
}

if (-not (Test-Path $UserConfig)) {
    Copy-Item -Path $DefaultConfig -Destination $UserConfig -Force
    Write-SetupLog "Created user config: $UserConfig"
} else {
    Write-SetupLog "User config already exists, leaving unchanged: $UserConfig"
}

Write-SetupLog "User config path (used by mozc_server): $UserConfig"

if ($PullModel) {
    $ollama = Get-Command ollama -ErrorAction SilentlyContinue
    if (-not $ollama) {
        Write-SetupLog "Ollama not found. Install from https://ollama.ai and run: ollama pull gemma3:1b"
        exit 0
    }

    try {
        $config = Get-Content $UserConfig -Raw | ConvertFrom-Json
        $model = if ($config.ollama_model) { $config.ollama_model } else { "gemma3:1b" }
        Write-SetupLog "Pulling model: $model"
        & ollama pull $model
    } catch {
        Write-SetupLog "Model pull skipped: $_"
    }
}

exit 0
