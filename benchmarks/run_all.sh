#!/usr/bin/env bash
# Regenerates every benchmark number, result picture, figure and table in the report.
#
#   benchmarks/run_all.sh [build dir]
#
# Steps:
#   1. build the benchmark program and the tests (Release, -O2); run the tests
#   2. run all experiments          -> results/csv/*.csv and results/<image>/*.png
#   3. plot                         -> report/figures/*.png, report/generated/*
# Afterwards run report/build_report.sh to rebuild the PDF.
#
# Environment variables:
#   PYTHON   Python interpreter with the packages of benchmarks/requirements.txt
#            (default: python3)
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$REPO_DIR/build}"
PYTHON="${PYTHON:-python3}"

cmake -S "$REPO_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --target seam_bench seam_tests -j

# The report quotes the number of tests, so run them and keep the summary line.
"$BUILD_DIR/seam_tests" | tail -n 2 > "$REPO_DIR/results/test_summary.txt"

# Record the machine, so the timings in the report can be put into context.
{
    echo "cpu=$(grep -m1 'model name' /proc/cpuinfo 2>/dev/null | cut -d: -f2 | sed 's/^ //' || echo unknown)"
    echo "logical_cores=$(nproc 2>/dev/null || echo unknown)"
    echo "compiler=$(c++ --version | head -n 1)"
    echo "os=$(uname -sr)"
} > "$REPO_DIR/results/machine.txt"

START_SECONDS=$SECONDS
"$BUILD_DIR/seam_bench" all "$REPO_DIR/data/inputs" "$REPO_DIR/results"
echo "benchmark_minutes=$(( (SECONDS - START_SECONDS + 30) / 60 ))" >> "$REPO_DIR/results/machine.txt"
"$PYTHON" "$REPO_DIR/benchmarks/plot_results.py" "$REPO_DIR/results" "$REPO_DIR/report"

echo "Benchmarks done. Now run report/build_report.sh to rebuild the PDF."
