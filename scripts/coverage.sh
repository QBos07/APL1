#!/usr/bin/env bash
# Optional GCC/gcov coverage baseline for the project database layer (2.15).
#
# Uses a fresh temporary build directory with coverage flags, runs the
# SQLite-backed unit/integration test executable, and prints a gcov summary for
# src/db. It never changes the normal build or CTest configuration.
#
# Environment overrides:
#   APL1_COVERAGE_BUILD_DIR   use/keep this build directory instead of a temp one
#   TMPDIR                    base for the default temporary build directory

set -euo pipefail

REPO_ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"

usage() {
    sed -n '2,11p' "$0" | sed 's/^# \{0,1\}//'
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    usage
    exit 0
fi
if [[ $# -gt 0 ]]; then
    echo "coverage: unexpected argument: $1" >&2
    usage >&2
    exit 2
fi

TEMP_BUILD_DIR=""
CLEANUP_REQUIRED=0

cleanup() {
    if [[ "$CLEANUP_REQUIRED" -eq 1 && -n "$TEMP_BUILD_DIR" ]]; then
        rm -rf -- "$TEMP_BUILD_DIR"
    fi
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

if [[ -n "${APL1_COVERAGE_BUILD_DIR:-}" ]]; then
    BUILD_DIR="$APL1_COVERAGE_BUILD_DIR"
    if [[ "$BUILD_DIR" != /* ]]; then
        BUILD_DIR="$REPO_ROOT/$BUILD_DIR"
    fi
    CLEANUP_REQUIRED=0
    echo "coverage: using existing build directory $BUILD_DIR (not removed)" >&2
else
    TMP_BASE="${TMPDIR:-/tmp}"
    TEMP_BUILD_DIR="$(mktemp -d "$TMP_BASE/apl1-coverage-XXXXXX")"
    BUILD_DIR="$TEMP_BUILD_DIR"
    CLEANUP_REQUIRED=1
    echo "coverage: using fresh build directory $BUILD_DIR" >&2
fi

if ! command -v gcov >/dev/null 2>&1; then
    echo "coverage: gcov is not installed" >&2
    exit 2
fi

echo "==> Configure (coverage)" >&2
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
    -DBUILD_TESTING=ON \
    -DCMAKE_BUILD_TYPE=Debug \
    '-DCMAKE_C_FLAGS=--coverage -O0 -g' \
    '-DCMAKE_EXE_LINKER_FLAGS=--coverage'

echo "==> Build" >&2
cmake --build "$BUILD_DIR" --parallel

echo "==> Test (apl1_unit_tests)" >&2
ctest --test-dir "$BUILD_DIR" -R apl1_unit_tests --output-on-failure

GCNO_DIR="$BUILD_DIR/CMakeFiles/apl_db.dir/src/db"
if ! compgen -G "$GCNO_DIR/*.gcno" >/dev/null; then
    echo "coverage: no database coverage data found in $GCNO_DIR" >&2
    exit 1
fi

REPORT_DIR="$BUILD_DIR/coverage-db"
rm -rf -- "$REPORT_DIR"
mkdir -p "$REPORT_DIR"

echo "==> gcov (src/db)" >&2
(
    cd "$REPORT_DIR"
    gcov -b -c "$GCNO_DIR"/*.gcno
) | tee "$REPORT_DIR/summary.txt"

echo "coverage: report files are in $REPORT_DIR" >&2
