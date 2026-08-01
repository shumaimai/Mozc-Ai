# AI Mozc IME - post-install setup
# Seeds user config from the installed default template.

param(
    [string]$InstallDir = "$env:ProgramFiles\Mozc",
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

$ConfigDir = Join-Path $env:LOCALAPPDATA "Google\Mozc"
$UserConfig = Join-Path $ConfigDir "ai_config.json"
$DefaultConfig = Join-Path $InstallDir "ai_config.default.json"

if (-not (Test-Path $DefaultConfig)) {
    Write-SetupLog "Default config not found: $DefaultConfig"
    exit 0
}

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
