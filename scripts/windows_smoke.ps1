# Mozc AI 1.0.3-test1 real-machine smoke test (SYNTHETIC INPUT ONLY).
#
# Sends only fixed synthetic strings to the local rerank daemon.  No user
# input is read, recorded, or transmitted anywhere.  Safe to paste output.
#
# Usage (regular PowerShell, no admin needed):
#   powershell -ExecutionPolicy Bypass -File scripts\windows_smoke.ps1
#   powershell ... -LatencyRuns 500 -InstallDir "C:\Program Files\Mozc"

param(
    [string]$InstallDir = "$env:ProgramFiles\Mozc",
    [int]$Port = 17890,
    [int]$LatencyRuns = 200,
    [int]$TimeoutMs = 200
)

$ErrorActionPreference = "Stop"
$ModelDir = Join-Path $InstallDir "ai\model"
$Daemon = Join-Path $InstallDir "ai\rerank_daemon.exe"

function Fail([string]$msg) {
    Write-Host "SMOKE FAIL: $msg" -ForegroundColor Red
    exit 1
}

Write-Host "== 1. Installed payload check =="
foreach ($p in @($Daemon, (Join-Path $ModelDir "cross_encoder_fp32.onnx"), (Join-Path $ModelDir "SHA256SUMS"), (Join-Path $ModelDir "margin_policy.json"))) {
    if (-not (Test-Path -LiteralPath $p)) { Fail "missing: $p" }
}
$policy = Get-Content -Raw (Join-Path $ModelDir "margin_policy.json") | ConvertFrom-Json
if ($policy.guard_mode -ne "safety") { Fail "margin_policy.json guard_mode=$($policy.guard_mode), expected safety" }
Write-Host "policy guard_mode=safety OK (tau=$($policy.tau) timeout_ms=$($policy.timeout_ms))"

$sumsPath = Join-Path $ModelDir "SHA256SUMS"
foreach ($line in Get-Content -LiteralPath $sumsPath) {
    if (-not $line.Trim()) { continue }
    $parts = $line -split '\s+', 2
    $expected = $parts[0].ToLowerInvariant()
    $rel = $parts[1].Trim().TrimStart('*')
    $file = Join-Path $ModelDir ($rel -replace '/', '\')
    if (-not (Test-Path -LiteralPath $file)) { Fail "SHA256SUMS entry missing on disk: $rel" }
    $actual = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $expected) { Fail "hash mismatch: $rel (expected $expected, got $actual)" }
}
Write-Host "SHA256SUMS all matched (installed payload == frozen Phase 2 model)"

$onnxLen = (Get-Item (Join-Path $ModelDir "cross_encoder_fp32.onnx")).Length
if ($onnxLen -lt 100MB) { Fail "ONNX suspiciously small ($onnxLen bytes)" }
Write-Host "ONNX size OK: $onnxLen bytes"

Write-Host "== 2. Daemon process / port =="
$proc = Get-Process -Name "rerank_daemon" -ErrorAction SilentlyContinue
if ($proc) {
    Write-Host "rerank_daemon.exe running (pid $($proc.Id -join ','))"
} else {
    Write-Host "rerank_daemon.exe NOT running - starting from $Daemon" -ForegroundColor Yellow
    Start-Process -FilePath $Daemon -WindowStyle Hidden
    Start-Sleep -Seconds 10
    if (-not (Get-Process -Name "rerank_daemon" -ErrorAction SilentlyContinue)) {
        Fail "daemon did not start; run it manually and check Event Viewer"
    }
}
$listen = netstat -ano | Select-String ":$Port\s.*LISTENING"
if (-not $listen) { Fail "nothing listening on 127.0.0.1:$Port" }
Write-Host "port ${Port}: LISTENING"

function Invoke-Request([string]$json, [int]$timeoutMs) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $client = [Net.Sockets.TcpClient]::new("127.0.0.1", $Port)
    $client.ReceiveTimeout = $timeoutMs
    $client.SendTimeout = $timeoutMs
    try {
        $s = $client.GetStream()
        $b = [Text.Encoding]::UTF8.GetBytes($json + "`n")
        $s.Write($b, 0, $b.Length)
        $r = [IO.StreamReader]::new($s, [Text.Encoding]::UTF8)
        $line = $r.ReadLine()
        $r.Dispose()
        $sw.Stop()
        return [pscustomobject]@{ ms = $sw.Elapsed.TotalMilliseconds; line = $line }
    } finally { $client.Dispose() }
}

Write-Host "== 3. Synthetic requests (fixed strings only) =="
$ping = Invoke-Request '{"op":"ping"}' 10000
if ($ping.line -notmatch '"op"\s*:\s*"pong"') { Fail "ping failed: $($ping.line)" }
$pingSha = ($ping.line | ConvertFrom-Json).model_sha256
if (-not $pingSha -or $pingSha -notmatch '^[0-9a-f]{64}$') { Fail "ping lacks valid model_sha256: $pingSha" }
Write-Host "ping OK ($([math]::Round($ping.ms,1)) ms) model_sha256=$pingSha"

# Scored synthetic conversion: model must actually run. req_id is an
# anonymous correlation id (no user content) echoed back for round-trip proof.
$r1 = Invoke-Request '{"req_id":777,"reading":"きしゃ","context_prev":"駅に","nbest":["記者","汽車"]}' ($TimeoutMs * 3)
$j1 = $r1.line | ConvertFrom-Json
Write-Host "scored:  ok=$($j1.ok) guard_skip=$($j1.guard_skip) overwritten=$($j1.overwritten) final_top1=$($j1.final_top1) margin=$($j1.margin) infer_ms=$($j1.infer_ms) req_id=$($j1.req_id) ($([math]::Round($r1.ms,1)) ms)"
if ($j1.ok -ne $true) { Fail "ok != true" }
if ($j1.guard_skip -ne $false) { Fail "guard_skip=true on scored request - the model did NOT run (packaging broken)" }
if ($null -eq $j1.margin) { Fail "no margin in response - scoring did not execute" }
if ($j1.req_id -ne 777) { Fail "req_id echo missing - anonymous round-trip proof broken" }
if ($null -eq $j1.infer_ms) { Fail "infer_ms missing - daemon timing diagnostics broken" }

# Guard skip: short reading must never reach the model.
$r2 = Invoke-Request '{"reading":"い","context_prev":"文化","nbest":["位","李"]}' 10000
$j2 = $r2.line | ConvertFrom-Json
Write-Host "guard:   ok=$($j2.ok) guard_skip=$($j2.guard_skip) reason=$($j2.reason)"
if ($j2.guard_skip -ne $true) { Fail "expected guard_skip=true for short reading" }

# Timeout/fail-safe behavior of the C++ side is covered by the CI bazel test
# TimeoutKeepsMozcOrder (daemon dead / slow => Mozc keeps native order).

Write-Host "== 4. Latency: $LatencyRuns synthetic conversions =="
$latencies = [System.Collections.Generic.List[double]]::new()
$timeouts = 0
for ($i = 0; $i -lt $LatencyRuns; $i++) {
    $ctx = if ($i % 2 -eq 0) { "駅に" } else { "新聞の" }
    try {
        $resp = Invoke-Request ('{"reading":"きしゃ","context_prev":"' + $ctx + '","nbest":["記者","汽車","貴社"]}') ($TimeoutMs * 3)
        $null = $resp.line | ConvertFrom-Json
        $latencies.Add($resp.ms)
        if ($resp.ms -gt $TimeoutMs) { $timeouts++ }
    } catch {
        $timeouts++
    }
}
if ($latencies.Count -eq 0) { Fail "no successful latency samples" }
$sorted = $latencies | Sort-Object
function Pct([double]$p) {
    $idx = [math]::Ceiling($p * $sorted.Count) - 1
    if ($idx -lt 0) { $idx = 0 }
    return [math]::Round($sorted[[int]$idx], 1)
}
Write-Host ""
Write-Host "samples=$($latencies.Count) timeouts(>${TimeoutMs}ms)=$timeouts"
Write-Host ("p50={0}ms p95={1}ms p99={2}ms max={3}ms mean={4}ms" -f (Pct 0.5), (Pct 0.95), (Pct 0.99), [math]::Round(($sorted | Select-Object -Last 1), 1), [math]::Round(($latencies | Measure-Object -Average).Average, 1))
Write-Host ""
if ($timeouts -gt 0) {
    Write-Host "WARN: $timeouts requests exceeded ${TimeoutMs}ms - check CPU load / power plan" -ForegroundColor Yellow
}
Write-Host "SMOKE PASS" -ForegroundColor Green
Write-Host "(Output contains synthetic strings only - safe to share/paste.)"
