#!/usr/bin/env bash
# Clean configure -> build -> CTest check for apl1 (FH-03).
#
# By default a fresh temporary build directory is created beneath TMPDIR and
# removed on exit. An explicit build directory is never removed implicitly.
#
# Environment overrides:
#   APL1_CHECK_BUILD_DIR   use this build directory instead of a temporary one
#                          (it is kept after the run)
#   APL1_CHECK_CMAKE_ARGS  extra arguments passed to the configure step
#   TMPDIR                 base for the default temporary build directory
#
# Examples:
#   scripts/check.sh
#   APL1_CHECK_BUILD_DIR=build scripts/check.sh
#   APL1_CHECK_CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Debug" scripts/check.sh

set -euo pipefail

REPO_ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"

usage() {
    sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    usage
    exit 0
fi
if [[ $# -gt 0 ]]; then
    echo "check: unexpected argument: $1" >&2
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

if [[ -n "${APL1_CHECK_BUILD_DIR:-}" ]]; then
    BUILD_DIR="$APL1_CHECK_BUILD_DIR"
    if [[ "$BUILD_DIR" != /* ]]; then
        BUILD_DIR="$REPO_ROOT/$BUILD_DIR"
    fi
    if [[ "$BUILD_DIR" == "$REPO_ROOT" ]]; then
        echo "check: refusing to use the repository root as the build directory" >&2
        exit 2
    fi
    CLEANUP_REQUIRED=0
    echo "check: using existing build directory $BUILD_DIR (not removed)" >&2
else
    TMP_BASE="${TMPDIR:-/tmp}"
    TEMP_BUILD_DIR="$(mktemp -d "$TMP_BASE/apl1-check-XXXXXX")"
    BUILD_DIR="$TEMP_BUILD_DIR"
    CLEANUP_REQUIRED=1
    echo "check: using fresh build directory $BUILD_DIR" >&2
fi

run_stage() {
    local stage="$1"
    shift
    echo "==> $stage" >&2
    set +e
    "$@"
    local status=$?
    set -e
    if [[ $status -ne 0 ]]; then
        echo "check: $stage failed (exit $status)" >&2
        exit "$status"
    fi
}

# APL1_CHECK_CMAKE_ARGS is intentionally word-split so callers can pass
# multiple simple flags. Values containing spaces are not supported.
# shellcheck disable=SC2086
run_stage "Configure" cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
    -DBUILD_TESTING=ON ${APL1_CHECK_CMAKE_ARGS:-}

run_stage "Build" cmake --build "$BUILD_DIR" --parallel

run_stage "Test" ctest --test-dir "$BUILD_DIR" --output-on-failure

echo "check: all stages passed" >&2
