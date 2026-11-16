#!/usr/bin/env bash
# Rebuilds slides/Team5_Seam_Carving.pptx and slides/speaker_notes.md.
# Needs the figures and numbers produced by benchmarks/run_all.sh.
#   PYTHON   interpreter with python-pptx installed (default: python3)
set -euo pipefail
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"${PYTHON:-python3}" "$REPO_DIR/slides/build_slides.py" "$REPO_DIR"
