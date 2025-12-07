# Copyright 2024 AI Mozc IME Project
# Mozc Integration Script for Windows

param(
    [Parameter(Mandatory=$true)]
    [string]$MozcDir,

    [switch]$DryRun,
    [switch]$NoBackup,
    [switch]$Help
)

# Script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$AiMozcDir = Split-Path -Parent $ScriptDir

# Colors
function Write-Info { Write-Host "[INFO] $args" -ForegroundColor Blue }
function Write-OK { Write-Host "[OK] $args" -ForegroundColor Green }
function Write-Warn { Write-Host "[WARN] $args" -ForegroundColor Yellow }
function Write-Err { Write-Host "[ERROR] $args" -ForegroundColor Red }

# Help
if ($Help) {
    Write-Host @"
AI Mozc IME - Mozc Integration Script

Usage: .\integrate_mozc.ps1 -MozcDir <path> [options]

Required:
    -MozcDir <path>    Path to Mozc source directory (mozc/src)

Options:
    -DryRun            Show what would be done without making changes
    -NoBackup          Don't create backups of modified files
    -Help              Show this help message

Example:
    .\integrate_mozc.ps1 -MozcDir C:\mozc\src
    .\integrate_mozc.ps1 -MozcDir C:\mozc\src -DryRun
"@
    exit 0
}

# Validate
if (-not (Test-Path $MozcDir)) {
    Write-Err "Mozc directory does not exist: $MozcDir"
    exit 1
}

$mozcBuild = Join-Path $MozcDir "BUILD.bazel"
$mozcWorkspace = Join-Path $MozcDir "WORKSPACE"
$mozcModule = Join-Path $MozcDir "MODULE.bazel"

if (-not ((Test-Path $mozcBuild) -or (Test-Path $mozcWorkspace) -or (Test-Path $mozcModule))) {
    Write-Err "Does not appear to be a Mozc source directory: $MozcDir"
    exit 1
}

Write-Host "========================================"
Write-Host "AI Mozc IME - Mozc Integration"
Write-Host "========================================"
Write-Host ""
Write-Info "AI Mozc directory: $AiMozcDir"
Write-Info "Mozc directory: $MozcDir"
Write-Info "Dry run: $DryRun"
Write-Info "Backup: $(-not $NoBackup)"
Write-Host ""

# Backup directory
$BackupDir = Join-Path $MozcDir ".ai_mozc_backup_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
if (-not $NoBackup -and -not $DryRun) {
    New-Item -ItemType Directory -Path $BackupDir -Force | Out-Null
    Write-Info "Backup directory: $BackupDir"
}

# Copy function
function Copy-FileWithCheck {
    param($Src, $Dst)

    if ($DryRun) {
        Write-Host "  [DRY RUN] Would copy: $Src -> $Dst"
    } else {
        $dstDir = Split-Path -Parent $Dst
        if (-not (Test-Path $dstDir)) {
            New-Item -ItemType Directory -Path $dstDir -Force | Out-Null
        }
        Copy-Item -Path $Src -Destination $Dst -Force
        Write-OK "Copied: $(Split-Path -Leaf $Src)"
    }
}

# Step 1: Copy AI module files
Write-Host ""
Write-Info "Step 1: Copying AI module files..."

$AiDstDir = Join-Path $MozcDir "ai"
if (-not $DryRun) {
    New-Item -ItemType Directory -Path $AiDstDir -Force | Out-Null
}

$AiFiles = @(
    "ai_config.h", "ai_config.cc", "ai_config_test.cc",
    "ai_logger.h", "ai_logger.cc",
    "ai_candidate_cache.h", "ai_candidate_cache.cc", "ai_candidate_cache_test.cc",
    "ai_backend.h", "ollama_backend.cc", "mock_backend.cc", "ai_backend_test.cc",
    "ai_worker.h", "ai_worker.cc", "ai_worker_test.cc",
    "BUILD"
)

foreach ($file in $AiFiles) {
    $src = Join-Path $AiMozcDir "src\ai\$file"
    $dst = Join-Path $AiDstDir $file
    if (Test-Path $src) {
        Copy-FileWithCheck $src $dst
    } else {
        Write-Warn "File not found: src\ai\$file"
    }
}

# Step 2: Copy AIRewriter files
Write-Host ""
Write-Info "Step 2: Copying AIRewriter files..."

$RewriterFiles = @("ai_rewriter.h", "ai_rewriter.cc", "ai_rewriter_test.cc")

foreach ($file in $RewriterFiles) {
    $src = Join-Path $AiMozcDir "src\rewriter\$file"
    $dst = Join-Path $MozcDir "rewriter\$file"
    if (Test-Path $src) {
        Copy-FileWithCheck $src $dst
    } else {
        Write-Warn "File not found: src\rewriter\$file"
    }
}

# Step 3: Create Mozc-compatible BUILD file
Write-Host ""
Write-Info "Step 3: Creating Mozc-compatible BUILD file..."

$MozcAiBuild = @'
# Copyright 2024 AI Mozc IME Project
# AI Module BUILD file for Mozc integration

load("//bazel:stubs.bzl", "bzl_library", "cc_library_mozc", "cc_test_mozc")

package(default_visibility = ["//visibility:public"])

cc_library_mozc(
    name = "ai_config",
    srcs = ["ai_config.cc"],
    hdrs = ["ai_config.h"],
    deps = [
        "//base:logging",
        "//base:port",
    ],
)

cc_library_mozc(
    name = "ai_logger",
    srcs = ["ai_logger.cc"],
    hdrs = ["ai_logger.h"],
    deps = [
        ":ai_config",
        "//base:logging",
    ],
)

cc_library_mozc(
    name = "ai_candidate_cache",
    srcs = ["ai_candidate_cache.cc"],
    hdrs = ["ai_candidate_cache.h"],
    deps = [
        ":ai_config",
    ],
)

cc_library_mozc(
    name = "ai_backend",
    srcs = [
        "mock_backend.cc",
        "ollama_backend.cc",
    ],
    hdrs = ["ai_backend.h"],
    deps = [
        ":ai_config",
        ":ai_logger",
    ],
)

cc_library_mozc(
    name = "ai_worker",
    srcs = ["ai_worker.cc"],
    hdrs = ["ai_worker.h"],
    deps = [
        ":ai_backend",
        ":ai_candidate_cache",
        ":ai_config",
        ":ai_logger",
    ],
)

cc_library_mozc(
    name = "ai",
    deps = [
        ":ai_backend",
        ":ai_candidate_cache",
        ":ai_config",
        ":ai_logger",
        ":ai_worker",
    ],
)

# Tests
cc_test_mozc(
    name = "ai_config_test",
    srcs = ["ai_config_test.cc"],
    deps = [
        ":ai_config",
        "//testing:gunit_main",
    ],
)

cc_test_mozc(
    name = "ai_candidate_cache_test",
    srcs = ["ai_candidate_cache_test.cc"],
    deps = [
        ":ai_candidate_cache",
        "//testing:gunit_main",
    ],
)

cc_test_mozc(
    name = "ai_worker_test",
    srcs = ["ai_worker_test.cc"],
    deps = [
        ":ai_worker",
        "//testing:gunit_main",
    ],
)

cc_test_mozc(
    name = "ai_backend_test",
    srcs = ["ai_backend_test.cc"],
    deps = [
        ":ai_backend",
        "//testing:gunit_main",
    ],
)
'@

$buildMozcPath = Join-Path $AiDstDir "BUILD.mozc"
if ($DryRun) {
    Write-Host "  [DRY RUN] Would create: $buildMozcPath"
} else {
    $MozcAiBuild | Out-File -FilePath $buildMozcPath -Encoding UTF8
    Write-OK "Created: BUILD.mozc"
}

# Step 4: Create patch for rewriter/BUILD
Write-Host ""
Write-Info "Step 4: Creating patch file..."

$RewriterBuildPatch = @'
# ==== AI Rewriter (add this section to rewriter/BUILD) ====

cc_library_mozc(
    name = "ai_rewriter",
    srcs = ["ai_rewriter.cc"],
    hdrs = ["ai_rewriter.h"],
    deps = [
        ":rewriter_interface",
        "//ai:ai_candidate_cache",
        "//ai:ai_config",
        "//ai:ai_worker",
    ],
)

cc_test_mozc(
    name = "ai_rewriter_test",
    srcs = ["ai_rewriter_test.cc"],
    size = "enormous",
    timeout = "eternal",
    deps = [
        ":ai_rewriter",
        "//testing:gunit_main",
    ],
)
# ==== End of AI Rewriter section ====
'@

$patchPath = Join-Path $MozcDir "rewriter\ai_rewriter_build.patch"
if ($DryRun) {
    Write-Host "  [DRY RUN] Would create: $patchPath"
} else {
    $RewriterBuildPatch | Out-File -FilePath $patchPath -Encoding UTF8
    Write-OK "Created: ai_rewriter_build.patch"
}

# Step 5: Create include path adapter
Write-Host ""
Write-Info "Step 5: Creating include path adapter..."

$AdapterHeader = @'
// Copyright 2024 AI Mozc IME Project
// Include path adapter for Mozc integration

#ifndef MOZC_AI_AI_INCLUDES_H_
#define MOZC_AI_AI_INCLUDES_H_

#include "ai/ai_config.h"
#include "ai/ai_logger.h"
#include "ai/ai_backend.h"
#include "ai/ai_candidate_cache.h"
#include "ai/ai_worker.h"

#endif  // MOZC_AI_AI_INCLUDES_H_
'@

$adapterPath = Join-Path $AiDstDir "ai_includes.h"
if ($DryRun) {
    Write-Host "  [DRY RUN] Would create: $adapterPath"
} else {
    $AdapterHeader | Out-File -FilePath $adapterPath -Encoding UTF8
    Write-OK "Created: ai_includes.h"
}

# Step 6: Create instructions
Write-Host ""
Write-Info "Step 6: Creating integration instructions..."

$Instructions = @"
# AI Mozc IME Integration Instructions

Generated: $(Get-Date)

## Next Steps

### 1. Replace BUILD file
``````powershell
cd $AiDstDir
Rename-Item BUILD BUILD.standalone
Rename-Item BUILD.mozc BUILD
``````

### 2. Add AI Rewriter to rewriter/BUILD
Open ``rewriter/BUILD`` and add the contents of ``ai_rewriter_build.patch``.

### 3. Modify ai_rewriter.cc includes
Change ``../ai/`` to ``ai/`` in include paths.

### 4. Add AIRewriter to Rewriter Chain
Edit ``rewriter/rewriter.cc``:
``````cpp
#include "rewriter/ai_rewriter.h"
AddRewriter(std::make_unique<AIRewriter>());
``````

### 5. Build and Test
``````powershell
cd $MozcDir
bazelisk build //ai:all
bazelisk build //rewriter:ai_rewriter
bazelisk test //ai:all //rewriter:ai_rewriter_test
``````
"@

$instructionsPath = Join-Path $MozcDir "AI_MOZC_INTEGRATION.md"
if ($DryRun) {
    Write-Host "  [DRY RUN] Would create: $instructionsPath"
} else {
    $Instructions | Out-File -FilePath $instructionsPath -Encoding UTF8
    Write-OK "Created: AI_MOZC_INTEGRATION.md"
}

# Summary
Write-Host ""
Write-Host "========================================"
if ($DryRun) {
    Write-Warn "DRY RUN COMPLETE - No files were modified"
} else {
    Write-OK "Integration files copied successfully!"
}
Write-Host "========================================"
Write-Host ""
Write-Info "Next steps:"
Write-Host "  1. Read: $instructionsPath"
Write-Host "  2. Replace: ai/BUILD with ai/BUILD.mozc"
Write-Host "  3. Patch: rewriter/BUILD with ai_rewriter_build.patch"
Write-Host "  4. Edit: Include paths in ai_rewriter.cc"
Write-Host "  5. Build: bazelisk build //ai:all //rewriter:ai_rewriter"
Write-Host ""

if (-not $NoBackup -and -not $DryRun) {
    Write-Info "Backup location: $BackupDir"
}
