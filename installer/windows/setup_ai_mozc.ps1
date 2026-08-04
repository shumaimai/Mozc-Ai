# AI Mozc IME - post-install setup
# Seeds user config from the installed default template.
# With -CleanInstall, always refreshes config and clears stale logs.

param(
    [string]$InstallDir = "",
    [switch]$CleanInstall,
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

function Invoke-RegistryFix {
    param([string]$PreferredRoot = "")
    $fixScript = Find-AIAssetPath -FileName "fix_mozc_registry.ps1" -PreferredRoot $PreferredRoot
    if ($fixScript) {
        Write-SetupLog "Fixing Mozc TIP registry path..."
        & $fixScript -Quiet:$Quiet
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
$LogFile = Join-Path $ConfigDir "ai_log.txt"

if (-not (Test-Path $ConfigDir)) {
    New-Item -ItemType Directory -Path $ConfigDir -Force | Out-Null
    Write-SetupLog "Created config directory: $ConfigDir"
}

if ($CleanInstall -or -not (Test-Path $UserConfig)) {
    Copy-Item -Path $DefaultConfig -Destination $UserConfig -Force
    if ($CleanInstall) {
        Write-SetupLog "Refreshed user config (clean install): $UserConfig"
    } else {
        Write-SetupLog "Created user config: $UserConfig"
    }
} else {
    Write-SetupLog "User config already exists, leaving unchanged: $UserConfig"
    Write-SetupLog "Tip: use -CleanInstall to overwrite from template"
}

if ($CleanInstall -and (Test-Path $LogFile)) {
    Remove-Item -Path $LogFile -Force -ErrorAction SilentlyContinue
    Write-SetupLog "Cleared stale log for fresh verification"
}

$markerFile = Join-Path $ConfigDir "install_ready.txt"
@(
    "Mozc AI install setup complete",
    "Timestamp: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')",
    "Config: $UserConfig",
    "Next: Reboot PC, then type in IME and check ai_log.txt"
) | Set-Content -Path $markerFile -Encoding UTF8

Write-SetupLog "User config path (used by mozc_server): $UserConfig"

Invoke-RegistryFix -PreferredRoot $InstallDir

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
