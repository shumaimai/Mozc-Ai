# Mozc AI v1.0 - reproducible all-in-one Windows MSI packager.

param(
    [string]$MozcDir = "",
    [string]$MozcRepo = "https://github.com/google/mozc.git",
    [string]$MozcRef = "3f235b4eb6fcff7d14ef5f0fb8ee56de7ee4c732",
    [string]$OutputDir = "",
    [switch]$SkipRuntime,
    [switch]$SkipDeps,
    [switch]$SkipQt,
    [switch]$DryRun,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

function Show-Help {
    Write-Host @"
Mozc AI v1.0 - Windows MSI Packager

Usage: .\package_windows.ps1 [options]

Options:
    -MozcDir <path>     Existing mozc/src directory (if omitted, clones to .mozc-build)
    -MozcRepo <url>     Mozc git repository URL
    -MozcRef <ref>       Pinned Mozc commit (default is the tested v1.0 base)
    -OutputDir <path>   Copy MSI here (default: dist\)
    -SkipRuntime         Reuse an already-built runtime\bundle
    -SkipDeps            Skip python build_tools/update_deps.py
    -SkipQt              Skip Qt build (only if already built)
    -DryRun              Show commands without executing integration/build
    -Help                Show this help

Prerequisites:
    - Visual Studio 2022 with C++ workload and Windows SDK
    - Python 3
    - Git
    - Bazelisk
    - .NET SDK (for WiX via Mozc)

Output:
    MozcAI-1.0.3-test1-x64.msi (TEST BUILD: Mozc + local AI runtime + Phase 2 model)

Example:
    .\package_windows.ps1
    .\package_windows.ps1 -MozcDir C:\src\mozc\src -OutputDir .\dist
"@
}

function Require-Command {
    param([string]$Name)
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "Required command not found: $Name"
    }
}

function Invoke-Step {
    param(
        [string]$Title,
        [scriptblock]$Action
    )
    Write-Host ""
    Write-Host "== $Title ==" -ForegroundColor Cyan
    if ($DryRun) {
        Write-Host "[dry-run] skipped"
        return
    }
    & $Action
    if ($LASTEXITCODE -ne 0) {
        throw "Step failed: $Title (exit $LASTEXITCODE)"
    }
}

if ($Help) {
    Show-Help
    exit 0
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
if (-not $OutputDir) {
    $OutputDir = Join-Path $ProjectRoot "dist"
}

Require-Command git
Require-Command python
Require-Command bazelisk

if (-not $SkipRuntime) {
    Invoke-Step "Build local-only AI runtime" {
        & (Join-Path $ScriptDir "build_runtime_bundle.ps1")
    }
}

if (-not $MozcDir) {
    $WorkRoot = Join-Path $ProjectRoot ".mozc-build"
    $CloneRoot = Join-Path $WorkRoot "mozc"
    $MozcDir = Join-Path $CloneRoot "src"

    Invoke-Step "Clone Mozc" {
        # actions/cache restores nested src/third_party_cache and src/third_party/qt
        # before this step. That can create CloneRoot without a Git repository.
        # Checking only Test-Path CloneRoot caused Git to discover the PARENT
        # Mozc-Ai checkout and check out upstream Mozc over the wrong worktree.
        $nestedGit = Join-Path $CloneRoot ".git"
        if (-not (Test-Path -LiteralPath $nestedGit)) {
            if (Test-Path -LiteralPath $CloneRoot) {
                Write-Host "Removing cache-created directory without its own .git"
                Remove-Item -LiteralPath $CloneRoot -Recurse -Force
            }
            git clone $MozcRepo $CloneRoot
            if ($LASTEXITCODE -ne 0) { throw "Mozc clone failed" }
        }
        Push-Location $CloneRoot
        try {
            # Refuse to run ANY mutating Git command if Git resolves to the
            # outer Mozc-Ai checkout instead of the nested Mozc repository.
            $actualRoot = (git rev-parse --show-toplevel).Trim()
            if ($LASTEXITCODE -ne 0 -or
                [IO.Path]::GetFullPath($actualRoot).TrimEnd('\', '/') -ine
                [IO.Path]::GetFullPath($CloneRoot).TrimEnd('\', '/')) {
                throw "Mozc clone root mismatch: expected $CloneRoot, got $actualRoot"
            }
            git remote set-url origin $MozcRepo
            if ($LASTEXITCODE -ne 0) { throw "Mozc origin setup failed" }
            Write-Host "Mozc origin: $(git remote get-url origin)"

            git cat-file -e "$MozcRef^{commit}" 2>$null
            if ($LASTEXITCODE -ne 0) {
                # Do not assume the remote branch is named master. In
                # particular, a direct SHA fetch can intermittently be denied
                # by the GitHub upload-pack endpoint. Fetch configured refs
                # first, then retry the exact pinned SHA if still absent.
                git fetch --no-tags origin
                if ($LASTEXITCODE -ne 0) {
                    Write-Warning "Mozc ordinary fetch failed; trying pinned SHA"
                }
                git cat-file -e "$MozcRef^{commit}" 2>$null
                if ($LASTEXITCODE -ne 0) {
                    $fetched = $false
                    for ($attempt = 1; $attempt -le 3; $attempt++) {
                        Write-Host "Fetching pinned Mozc commit (attempt $attempt/3)"
                        git fetch --no-tags origin $MozcRef
                        if ($LASTEXITCODE -eq 0) {
                            git cat-file -e "$MozcRef^{commit}" 2>$null
                            if ($LASTEXITCODE -eq 0) { $fetched = $true; break }
                        }
                        if ($attempt -lt 3) { Start-Sleep -Seconds (2 * $attempt) }
                    }
                    if (-not $fetched) {
                        throw "Pinned Mozc commit unavailable after ordinary fetch and 3 SHA attempts: $MozcRef"
                    }
                }
            }
            git checkout --detach $MozcRef
            if ($LASTEXITCODE -ne 0) { throw "Pinned Mozc checkout failed: $MozcRef" }
            $actual = (git rev-parse HEAD).Trim()
            if ($LASTEXITCODE -ne 0 -or $actual -ne $MozcRef) {
                throw "Pinned Mozc checkout mismatch: expected $MozcRef, actual $actual"
            }
            if (-not (Test-Path -LiteralPath (Join-Path $CloneRoot "src/MODULE.bazel"))) {
                throw "Pinned Mozc checkout is incomplete: src/MODULE.bazel is missing"
            }
        } finally {
            Pop-Location
        }
    }
}

if (-not (Test-Path (Join-Path $MozcDir "MODULE.bazel"))) {
    throw "Invalid Mozc directory: $MozcDir"
}

Invoke-Step "Integrate AI module" {
    python (Join-Path $ScriptDir "integrate_mozc.py") --mozc-dir $MozcDir
}

Invoke-Step "Integrate Windows installer assets" {
    python (Join-Path $ScriptDir "integrate_mozc_installer.py") --mozc-dir $MozcDir
}

Push-Location $MozcDir
try {
    if (-not $SkipDeps) {
        Invoke-Step "Download Mozc build dependencies" {
            python build_tools/update_deps.py
        }
    }

    if (-not $SkipQt) {
        Invoke-Step "Build Qt dependencies" {
            python build_tools/build_qt.py --release --confirm_license
        }
    }

    Invoke-Step "Test integrated reranker" {
        bazelisk test //rewriter:rerank_rewriter_test --config release_build
    }

    Invoke-Step "Build Mozc AI v1.0.3-test1 MSI" {
        bazelisk build package --config release_build
    }
}
finally {
    Pop-Location
}

$MsiPath = Join-Path $MozcDir "bazel-bin\win32\installer\MozcAI-1.0.3-test1-x64.msi"
if (-not $DryRun) {
    if (-not (Test-Path $MsiPath)) {
        throw "MSI not found at expected path: $MsiPath"
    }

    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
    $Dest = Join-Path $OutputDir "MozcAI-1.0.3-test1-x64.msi"
    Copy-Item -Path $MsiPath -Destination $Dest -Force

    Write-Host ""
    Write-Host "Package created:" -ForegroundColor Green
    Write-Host "  $Dest"
    Write-Host ""
    Write-Host "The MSI has its own Mozc AI product identity and migrates legacy Mozc."
    Write-Host "No conversion text logging is enabled by default."
}
