#!/bin/bash
# Copyright 2024 AI Mozc IME Project
# Mozc Integration Script - Automatically integrates AI module into Mozc source

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print functions
print_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
print_ok() { echo -e "${GREEN}[OK]${NC} $1"; }
print_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
print_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Script directory (where ai_mozc is located)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AI_MOZC_DIR="$(dirname "$SCRIPT_DIR")"

# Default values
MOZC_DIR=""
DRY_RUN=false
BACKUP=true

# Help message
show_help() {
    echo "AI Mozc IME - Mozc Integration Script"
    echo ""
    echo "Usage: $0 --mozc-dir <path> [options]"
    echo ""
    echo "Required:"
    echo "    --mozc-dir <path>    Path to Mozc source directory (mozc/src)"
    echo ""
    echo "Options:"
    echo "    --dry-run            Show what would be done without making changes"
    echo "    --no-backup          Don't create backups of modified files"
    echo "    --help               Show this help message"
    echo ""
    echo "Example:"
    echo "    $0 --mozc-dir ~/mozc/src"
    echo "    $0 --mozc-dir /path/to/mozc/src --dry-run"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --mozc-dir)
            MOZC_DIR="$2"
            shift 2
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --no-backup)
            BACKUP=false
            shift
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Validate arguments
if [ -z "$MOZC_DIR" ]; then
    print_error "Mozc directory not specified"
    show_help
    exit 1
fi

if [ ! -d "$MOZC_DIR" ]; then
    print_error "Mozc directory does not exist: $MOZC_DIR"
    exit 1
fi

# Check for required files
if [ ! -f "$MOZC_DIR/BUILD.bazel" ] && [ ! -f "$MOZC_DIR/WORKSPACE" ] && [ ! -f "$MOZC_DIR/MODULE.bazel" ]; then
    print_error "Does not appear to be a Mozc source directory: $MOZC_DIR"
    exit 1
fi

echo "========================================"
echo "AI Mozc IME - Mozc Integration"
echo "========================================"
echo ""
print_info "AI Mozc directory: $AI_MOZC_DIR"
print_info "Mozc directory: $MOZC_DIR"
print_info "Dry run: $DRY_RUN"
print_info "Backup: $BACKUP"
echo ""

# Create backup directory
BACKUP_DIR="$MOZC_DIR/.ai_mozc_backup_$(date +%Y%m%d_%H%M%S)"
if [ "$BACKUP" = true ] && [ "$DRY_RUN" = false ]; then
    mkdir -p "$BACKUP_DIR"
    print_info "Backup directory: $BACKUP_DIR"
fi

# Function to copy file with optional dry run
copy_file() {
    local src="$1"
    local dst="$2"

    if [ "$DRY_RUN" = true ]; then
        echo "  [DRY RUN] Would copy: $src -> $dst"
    else
        mkdir -p "$(dirname "$dst")"
        cp "$src" "$dst"
        print_ok "Copied: $(basename "$src")"
    fi
}

# Function to backup file
backup_file() {
    local file="$1"

    if [ -f "$file" ] && [ "$BACKUP" = true ] && [ "$DRY_RUN" = false ]; then
        local rel_path="${file#$MOZC_DIR/}"
        local backup_path="$BACKUP_DIR/$rel_path"
        mkdir -p "$(dirname "$backup_path")"
        cp "$file" "$backup_path"
    fi
}

# Step 1: Copy AI module files
echo ""
print_info "Step 1: Copying AI module files..."

# Create ai directory in Mozc
AI_DST_DIR="$MOZC_DIR/ai"
if [ "$DRY_RUN" = false ]; then
    mkdir -p "$AI_DST_DIR"
fi

# Copy AI module files
AI_FILES=(
    "ai_config.h"
    "ai_config.cc"
    "ai_config_test.cc"
    "ai_logger.h"
    "ai_logger.cc"
    "ai_candidate_cache.h"
    "ai_candidate_cache.cc"
    "ai_candidate_cache_test.cc"
    "ai_backend.h"
    "ollama_backend.cc"
    "mock_backend.cc"
    "ai_backend_test.cc"
    "ai_worker.h"
    "ai_worker.cc"
    "ai_worker_test.cc"
)

for file in "${AI_FILES[@]}"; do
    if [ -f "$AI_MOZC_DIR/src/ai/$file" ]; then
        copy_file "$AI_MOZC_DIR/src/ai/$file" "$AI_DST_DIR/$file"
    else
        print_warn "File not found: src/ai/$file"
    fi
done

# Copy AI BUILD file (will be modified)
copy_file "$AI_MOZC_DIR/src/ai/BUILD" "$AI_DST_DIR/BUILD"

# Step 2: Copy AIRewriter files
echo ""
print_info "Step 2: Copying AIRewriter files..."

REWRITER_FILES=(
    "ai_rewriter.h"
    "ai_rewriter.cc"
    "ai_rewriter_test.cc"
)

for file in "${REWRITER_FILES[@]}"; do
    if [ -f "$AI_MOZC_DIR/src/rewriter/$file" ]; then
        copy_file "$AI_MOZC_DIR/src/rewriter/$file" "$MOZC_DIR/rewriter/$file"
    else
        print_warn "File not found: src/rewriter/$file"
    fi
done

# Step 3: Create Mozc-compatible BUILD file for AI module
echo ""
print_info "Step 3: Creating Mozc-compatible BUILD file for AI module..."

MOZC_AI_BUILD='# Copyright 2024 AI Mozc IME Project
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
'

if [ "$DRY_RUN" = true ]; then
    echo "  [DRY RUN] Would create: $AI_DST_DIR/BUILD.mozc"
else
    echo "$MOZC_AI_BUILD" > "$AI_DST_DIR/BUILD.mozc"
    print_ok "Created: BUILD.mozc (Mozc-compatible BUILD file)"
fi

# Step 4: Create patch for rewriter/BUILD
echo ""
print_info "Step 4: Creating patch file for rewriter/BUILD..."

REWRITER_BUILD_PATCH='
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
'

PATCH_FILE="$MOZC_DIR/rewriter/ai_rewriter_build.patch"
if [ "$DRY_RUN" = true ]; then
    echo "  [DRY RUN] Would create: $PATCH_FILE"
else
    echo "$REWRITER_BUILD_PATCH" > "$PATCH_FILE"
    print_ok "Created: ai_rewriter_build.patch"
fi

# Step 5: Create include path adapter header
echo ""
print_info "Step 5: Creating include path adapter..."

ADAPTER_HEADER='// Copyright 2024 AI Mozc IME Project
// Include path adapter for Mozc integration
// This file maps AI Mozc includes to Mozc-style paths

#ifndef MOZC_AI_AI_INCLUDES_H_
#define MOZC_AI_AI_INCLUDES_H_

// When integrated into Mozc, the include paths change from:
//   "../ai/ai_config.h"  ->  "ai/ai_config.h"
//
// This header provides a central point for managing these paths.

// AI Module headers
#include "ai/ai_config.h"
#include "ai/ai_logger.h"
#include "ai/ai_backend.h"
#include "ai/ai_candidate_cache.h"
#include "ai/ai_worker.h"

#endif  // MOZC_AI_AI_INCLUDES_H_
'

if [ "$DRY_RUN" = true ]; then
    echo "  [DRY RUN] Would create: $AI_DST_DIR/ai_includes.h"
else
    echo "$ADAPTER_HEADER" > "$AI_DST_DIR/ai_includes.h"
    print_ok "Created: ai_includes.h"
fi

# Step 6: Create integration instructions
echo ""
print_info "Step 6: Creating integration instructions..."

INSTRUCTIONS="# AI Mozc IME Integration Instructions

Generated: $(date)

## Files Copied

### AI Module (\`ai/\`)
$(for f in "${AI_FILES[@]}"; do echo "- $f"; done)
- BUILD
- BUILD.mozc (Mozc-compatible version)
- ai_includes.h (adapter header)

### Rewriter Module (\`rewriter/\`)
$(for f in "${REWRITER_FILES[@]}"; do echo "- $f"; done)
- ai_rewriter_build.patch

## Next Steps

### 1. Replace BUILD file
\`\`\`bash
cd $MOZC_DIR/ai
mv BUILD BUILD.standalone
mv BUILD.mozc BUILD
\`\`\`

### 2. Add AI Rewriter to rewriter/BUILD
Open \`rewriter/BUILD\` and add the contents of \`ai_rewriter_build.patch\`
at the end of the file.

### 3. Modify ai_rewriter.cc includes
Edit \`rewriter/ai_rewriter.cc\` and change:
\`\`\`cpp
// From:
#include \"../ai/ai_config.h\"
#include \"../ai/ai_backend.h\"

// To:
#include \"ai/ai_config.h\"
#include \"ai/ai_backend.h\"
\`\`\`

### 4. Add AIRewriter to Rewriter Chain
Edit \`rewriter/rewriter.cc\`:
\`\`\`cpp
#include \"rewriter/ai_rewriter.h\"

// In AddRewriters() or equivalent:
AddRewriter(std::make_unique<AIRewriter>());
\`\`\`

### 5. Build and Test
\`\`\`bash
cd $MOZC_DIR
bazelisk build //ai:all
bazelisk build //rewriter:ai_rewriter
bazelisk test //ai:all //rewriter:ai_rewriter_test
\`\`\`

## Rollback

If you need to undo the integration:
\`\`\`bash
# Remove AI module
rm -rf $MOZC_DIR/ai

# Remove AI Rewriter files
rm $MOZC_DIR/rewriter/ai_rewriter.h
rm $MOZC_DIR/rewriter/ai_rewriter.cc
rm $MOZC_DIR/rewriter/ai_rewriter_test.cc
rm $MOZC_DIR/rewriter/ai_rewriter_build.patch

# Restore backed up files (if any)
$(if [ "$BACKUP" = true ]; then echo "cp -r $BACKUP_DIR/* $MOZC_DIR/"; else echo "# No backup was created"; fi)
\`\`\`
"

INSTRUCTIONS_FILE="$MOZC_DIR/AI_MOZC_INTEGRATION.md"
if [ "$DRY_RUN" = true ]; then
    echo "  [DRY RUN] Would create: $INSTRUCTIONS_FILE"
else
    echo "$INSTRUCTIONS" > "$INSTRUCTIONS_FILE"
    print_ok "Created: AI_MOZC_INTEGRATION.md"
fi

# Summary
echo ""
echo "========================================"
if [ "$DRY_RUN" = true ]; then
    print_warn "DRY RUN COMPLETE - No files were modified"
else
    print_ok "Integration files copied successfully!"
fi
echo "========================================"
echo ""
print_info "Next steps:"
echo "  1. Read: $MOZC_DIR/AI_MOZC_INTEGRATION.md"
echo "  2. Replace: ai/BUILD with ai/BUILD.mozc"
echo "  3. Patch: rewriter/BUILD with ai_rewriter_build.patch"
echo "  4. Edit: Include paths in ai_rewriter.cc"
echo "  5. Build: bazelisk build //ai:all //rewriter:ai_rewriter"
echo ""

if [ "$BACKUP" = true ] && [ "$DRY_RUN" = false ]; then
    print_info "Backup location: $BACKUP_DIR"
fi
