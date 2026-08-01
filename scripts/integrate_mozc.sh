#!/bin/bash
# Copyright 2024 AI Mozc IME Project
# Mozc Integration Script - wrapper for integrate_mozc.py

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "$SCRIPT_DIR/integrate_mozc.py" "$@"
