#!/usr/bin/env bash
# Rewrite maintained C sources and headers with the repository .clang-format.
# This is a developer convenience; scripts/check-format.sh is the read-only
# check used in review.

set -euo pipefail

REPO_ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"

if ! command -v "$CLANG_FORMAT" >/dev/null 2>&1; then
    echo "format: $CLANG_FORMAT not found; install clang-format or set CLANG_FORMAT" >&2
    exit 2
fi

cd "$REPO_ROOT"
mapfile -t FILES < <(git ls-files '*.c' '*.h' | grep -E '^(src|tests)/' || true)
if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "format: no C sources or headers found" >&2
    exit 2
fi

"$CLANG_FORMAT" -i --style=file "${FILES[@]}"
echo "format: rewrote ${#FILES[@]} files with $("$CLANG_FORMAT" --version)" >&2
