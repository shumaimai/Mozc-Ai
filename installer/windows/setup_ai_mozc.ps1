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

function Find-AIAssetPath {
    param(
        [string]$FileName,
        [string]$PreferredRoot = ""
    )

    $roots = @()
    if ($PreferredRoot) { $roots += $PreferredRoot }
    if ($env:ProgramFiles) { $roots += (Join-Path $env:ProgramFiles "Mozc") }
    if (${env:ProgramFiles(x86)}) { $roots += (Join-Path ${env:ProgramFiles(x86)} "Mozc") }

    foreach ($root in ($roots | Select-Object -Unique)) {
        foreach ($subdir in @("", "documents")) {
            $dir = if ($subdir) { Join-Path $root $subdir } else { $root }
            $path = Join-Path $dir $FileName
            if (Test-Path $path) {
                return $path
            }
        }
    }
    return $null
}

$DefaultConfig = Find-AIAssetPath -FileName "ai_config.default.json" -PreferredRoot $InstallDir

if (-not $DefaultConfig) {
    Write-SetupLog "Default config not found (checked Mozc root and Mozc\documents)"
    exit 0
}

Write-SetupLog "Using default config: $DefaultConfig"

$ConfigDir = Join-Path $env:LOCALAPPDATA "Google\Mozc"
$UserConfig = Join-Path $ConfigDir "ai_config.json"

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
