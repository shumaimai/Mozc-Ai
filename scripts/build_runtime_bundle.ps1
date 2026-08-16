param(
    [string]$Python = "py",
    [string]$ModelDir = "",
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
if (-not $ModelDir) { $ModelDir = Join-Path $ProjectRoot "runtime\model" }
if (-not $OutputDir) { $OutputDir = Join-Path $ProjectRoot "runtime\bundle" }
$Venv = Join-Path $ProjectRoot ".bundle-venv"
$VenvPython = Join-Path $Venv "Scripts\python.exe"

foreach ($required in @(
    (Join-Path $ModelDir "cross_encoder_fp32.onnx"),
    (Join-Path $ModelDir "margin_policy.json"),
    (Join-Path $ModelDir "tokenizer\tokenizer.json"),
    (Join-Path $ModelDir "tokenizer\tokenizer.model")
)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Missing runtime asset: $required" }
}

if (-not (Test-Path -LiteralPath $VenvPython)) {
    & $Python -3 -m venv $Venv
    if ($LASTEXITCODE -ne 0) { throw "Failed to create bundle venv" }
}
& $VenvPython -m pip install --disable-pip-version-check -r (Join-Path $ProjectRoot "runtime\requirements-bundle.txt")
if ($LASTEXITCODE -ne 0) { throw "Failed to install bundle dependencies" }

$BuildRoot = Join-Path $ProjectRoot ".runtime-build"
& $VenvPython -m PyInstaller `
    --noconfirm `
    --clean `
    --onedir `
    --name rerank_daemon `
    --distpath $BuildRoot `
    --workpath (Join-Path $BuildRoot "work") `
    --specpath $BuildRoot `
    --collect-all sentencepiece `
    (Join-Path $ProjectRoot "runtime\rerank_daemon.py")
if ($LASTEXITCODE -ne 0) { throw "PyInstaller failed" }

$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$ProjectPrefix = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\') + '\'
if (-not $OutputDir.StartsWith($ProjectPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputDir must stay inside the project: $OutputDir"
}
if (Test-Path -LiteralPath $OutputDir) {
    Remove-Item -LiteralPath $OutputDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
Copy-Item -Recurse -Force -Path (Join-Path $BuildRoot "rerank_daemon\*") -Destination $OutputDir
Copy-Item -Recurse -Force -Path $ModelDir -Destination (Join-Path $OutputDir "model")

& (Join-Path $OutputDir "rerank_daemon.exe") --help | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Bundled daemon smoke failed" }
Write-Host "RUNTIME_BUNDLE_READY $OutputDir"
