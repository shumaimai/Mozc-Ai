#!/bin/bash
# Copyright 2024 AI Mozc IME Project
# Linux/Unix Build Script

set -e

# Default values
RELEASE=false
CLEAN=false
TEST=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --release|-r)
            RELEASE=true
            shift
            ;;
        --clean|-c)
            CLEAN=true
            shift
            ;;
        --test|-t)
            TEST=true
            shift
            ;;
        --help|-h)
            echo "AI Mozc IME Build Script"
            echo ""
            echo "Usage: ./build.sh [options]"
            echo ""
            echo "Options:"
            echo "    --release, -r    Build in release mode (optimized)"
            echo "    --clean, -c      Clean build directory before building"
            echo "    --test, -t       Run unit tests after building"
            echo "    --help, -h       Show this help message"
            echo ""
            echo "Examples:"
            echo "    ./build.sh                 # Debug build"
            echo "    ./build.sh --release       # Release build"
            echo "    ./build.sh --clean --test  # Clean build and run tests"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Find bazel
if command -v bazelisk &> /dev/null; then
    BAZEL=bazelisk
elif command -v bazel &> /dev/null; then
    BAZEL=bazel
else
    echo "Error: Bazel or Bazelisk not found."
    echo "Please install bazelisk: https://github.com/bazelbuild/bazelisk"
    exit 1
fi

echo "========================================"
echo "AI Mozc IME Build"
echo "========================================"

# Set working directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_ROOT"
echo "Working directory: $PROJECT_ROOT"

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo ""
    echo "Cleaning build directory..."
    $BAZEL clean
fi

# Build configuration
if [ "$RELEASE" = true ]; then
    CONFIG="release_build"
else
    CONFIG="debug"
fi
echo ""
echo "Building with config: $CONFIG"

# Build
echo ""
echo "Building AI module..."
$BAZEL build --config=$CONFIG //src/ai:ai

echo ""
echo "Building AIRewriter..."
$BAZEL build --config=$CONFIG //src/rewriter:ai_rewriter

echo ""
echo "========================================"
echo "Build successful!"
echo "========================================"

# Run tests if requested
if [ "$TEST" = true ]; then
    echo ""
    echo "Running tests..."

    if $BAZEL test --config=$CONFIG //src/ai:all //src/rewriter:all; then
        echo "All tests passed!"
    else
        echo "Warning: Some tests failed"
    fi
fi

echo ""
echo "Done."
