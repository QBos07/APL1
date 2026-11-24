#!/usr/bin/env bash
# Optional clang-tidy baseline for production C sources (FH-09).
#
# This is a developer check, not a build dependency or a CI gate. It configures
# a compile-commands database, then analyzes src/ with the repository
# .clang-tidy configuration. Findings are reported as failures so a regression
# is visible; fix them or narrow the configuration with a documented reason.
#
# Environment overrides:
#   APL1_TIDY_BUILD_DIR  build directory used for compile_commands.json
#                        (default: $TMPDIR/apl1-tidy-build, kept between runs)
#   CLANG_TIDY           clang-tidy binary (default: clang-tidy)
#   RUN_CLANG_TIDY       run-clang-tidy driver (default: run-clang-tidy)

set -euo pipefail

REPO_ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
BUILD_DIR="${APL1_TIDY_BUILD_DIR:-${TMPDIR:-/tmp}/apl1-tidy-build}"
CLANG_TIDY="${CLANG_TIDY:-clang-tidy}"
RUN_CLANG_TIDY="${RUN_CLANG_TIDY:-run-clang-tidy}"

for tool in cmake "$CLANG_TIDY" "$RUN_CLANG_TIDY"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "check-static: $tool not found; this check is optional" >&2
        exit 2
    fi
done

echo "check-static: $("$CLANG_TIDY" --version | head -1)" >&2
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
    -DBUILD_TESTING=OFF \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null

# clang-tidy reads .clang-tidy from the source tree; do not override -checks
# here so the checked-in exclusions stay authoritative.
if ! "$RUN_CLANG_TIDY" \
    -p "$BUILD_DIR" \
    -j "$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN)" \
    -quiet \
    -warnings-as-errors '*' \
    -source-filter '^.*/src/.*\.c$' \
    -header-filter '^.*/src/.*\.(c|h)$'; then
    echo "check-static: clang-tidy reported findings; triage them or narrow .clang-tidy" >&2
    exit 1
fi

echo "check-static: no clang-tidy findings in production sources" >&2
