#!/usr/bin/env bash
# Check C formatting without modifying files (FH-08).
#
# Dependency policy: clang-format is an explicit optional tool, not a normal
# build dependency. This script is not part of scripts/check.sh or CI by
# default. The checked-in .clang-format targets clang-format 21; other major
# versions may report differences on otherwise valid code.

set -euo pipefail

REPO_ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"

if ! command -v "$CLANG_FORMAT" >/dev/null 2>&1; then
    echo "check-format: $CLANG_FORMAT not found; install clang-format or set CLANG_FORMAT" >&2
    exit 2
fi

cd "$REPO_ROOT"
mapfile -t FILES < <(git ls-files '*.c' '*.h' | grep -E '^(src|tests)/' || true)
if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "check-format: no C sources or headers found" >&2
    exit 2
fi

"$CLANG_FORMAT" --dry-run --Werror --style=file "${FILES[@]}"
echo "check-format: ${#FILES[@]} files are formatted with $("$CLANG_FORMAT" --version)" >&2
