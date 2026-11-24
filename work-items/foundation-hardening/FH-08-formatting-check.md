# FH-08 — Adopt and check a C formatter configuration

**Priority:** Before Phase 2 adds many source files  
**Status:** Done  
**Goal:** Establish a consistent formatting configuration while the codebase is still small.

## Work list

- [x] Select a project formatting base/style and add a checked-in `.clang-format`.
- [x] Pin/document the supported clang-format major version or clearly document version tolerance.
- [x] Format the current maintained C headers/sources and tests with that configuration in a dedicated change.
- [x] Add a script or documented command that checks formatting without rewriting files.
- [x] Document formatting check usage in CONTRIBUTING and optionally call it from `scripts/check.sh` only if clang-format is an explicit required dependency.

## Acceptance checks

- [x] The repository has one configuration usable from the project root.
- [x] Running the documented check on the formatted files succeeds without modifying them.
- [x] The check reports nonzero when a controlled formatting violation is introduced (verify locally and revert the fixture).
- [x] Formatting changes are behavior-neutral and do not mix with feature implementation.
- [x] The dependency policy for local/CI formatting checks is explicit.

## Implementation notes (2026-11-24)

- Added one project-root `.clang-format` matching the existing style: 4-space
  indent, 100-column limit, function braces on their own line,
  control-statement braces attached, right-aligned pointers, includes not
  reordered.
- Formatted the maintained C sources, headers, and tests in a formatting-only
  change (37 files).
- Added `scripts/check-format.sh` (read-only, `--dry-run --Werror`) and
  `scripts/format.sh` (rewrite). Neither is part of `scripts/check.sh`, and
  clang-format is documented as an explicit optional dependency.
- The configuration targets clang-format 21; exact output can differ across
  major versions, which is documented in CONTRIBUTING.

## Verification record (2026-11-24)

- `scripts/check-format.sh` reports all 37 files formatted with clang-format
  21.1.8.
- A controlled indentation violation made the check exit 1; restoring the file
  returned it to 0.
- Normal build and both CTest suites still pass after formatting.

## Dependencies / boundaries

- No dependency on Phase 2 code.
- Do not add a broad CI dependency silently; document or install the formatter intentionally.
