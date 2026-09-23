param(
    [string]$Python = "python",
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
    (Join-Path $ModelDir "tokenizer\tokenizer.model"),
    (Join-Path $ModelDir "SHA256SUMS")
)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Missing runtime asset: $required" }
}

# Reject Git LFS pointer stubs — a failed smudge must never ship a 130-byte model.
$onnxPath = Join-Path $ModelDir "cross_encoder_fp32.onnx"
$onnxLen = (Get-Item -LiteralPath $onnxPath).Length
if ($onnxLen -lt 100MB) { throw "ONNX model is unexpectedly small: $onnxLen bytes" }
$onnxHead = [byte[]]::new(32)
$stream = [IO.File]::OpenRead($onnxPath)
try { [void]$stream.Read($onnxHead, 0, $onnxHead.Length) } finally { $stream.Dispose() }
if ([Text.Encoding]::ASCII.GetString($onnxHead).StartsWith("version https://git-lfs")) {
    throw "ONNX model is a Git LFS pointer stub (git lfs smudge failed)"
}

# Pin every model file to runtime/model/SHA256SUMS.
foreach ($line in Get-Content -LiteralPath (Join-Path $ModelDir "SHA256SUMS")) {
    if (-not $line.Trim()) { continue }
    $parts = $line -split '\s+', 2
    $expected = $parts[0].ToLowerInvariant()
    $rel = $parts[1].Trim().TrimStart('*')
    $file = Join-Path $ModelDir ($rel -replace '/', '\')
    if (-not (Test-Path -LiteralPath $file)) { throw "SHA256SUMS entry missing: $rel" }
    $actual = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $expected) {
        throw "Model hash mismatch: $rel (expected $expected, got $actual)"
    }
}
Write-Host "MODEL_SHA256_OK $ModelDir"

if (-not (Test-Path -LiteralPath $VenvPython)) {
    & $Python -m venv $Venv
    if ($LASTEXITCODE -ne 0) { throw "Failed to create bundle venv" }
}
& $VenvPython -m pip install --disable-pip-version-check -r (Join-Path $ProjectRoot "runtime\requirements-bundle.txt")
if ($LASTEXITCODE -ne 0) { throw "Failed to install bundle dependencies" }

$BuildRoot = Join-Path $ProjectRoot ".runtime-build"
& $VenvPython -m PyInstaller `
    --noconfirm `
    --clean `
    --onedir `
    --noconsole `
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

$Smoke = Start-Process `
    -FilePath (Join-Path $OutputDir "rerank_daemon.exe") `
    -ArgumentList "--help" `
    -WindowStyle Hidden `
    -Wait `
    -PassThru
if ($Smoke.ExitCode -ne 0) { throw "Bundled daemon smoke failed" }
Write-Host "RUNTIME_BUNDLE_READY $OutputDir"
