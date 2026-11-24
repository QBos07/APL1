# FH-09 — Run a static-analysis baseline

**Priority:** Before or during early Phase 2  
**Status:** Done  
**Goal:** Use one available C static analyzer to inspect the small foundation codebase and surface actionable issues before it grows.

## Work list

- [x] Select one initial tool (`cppcheck` or `clang-tidy`) based on availability and C23 support in the development/CI environment.
- [x] Document a reproducible invocation and the exact source/build configuration analyzed.
- [x] Run the tool on production C sources, including the variadic error-formatting API and SQLite lifecycle code.
- [x] Triage every finding as valid, false positive, or deferred; create separate work items for fixes that are not trivial.
- [x] Add narrow suppressions only when justified and explain each one.
- [x] Decide whether this is an optional local check or a CI gate after observing noise and runtime.

## Acceptance checks

- [x] A contributor can reproduce the analysis using the documented command/tool version.
- [x] Findings and disposition are recorded; no broad “ignore all” suppression hides results.
- [x] Format-string call sites are checked by both compiler diagnostics and the selected analyzer where supported.
- [x] No tool is made a mandatory build dependency without an explicit project decision.

## Implementation notes (2026-11-24)

- Selected clang-tidy (21.1.8) because it is available and supports the C23
  build. Added `.clang-tidy` limited to `clang-analyzer-*`, `bugprone-*`,
  `cert-*`, `performance-*`, and `portability-*`.
- Exclusions are narrow and documented: standard-C `DeprecatedOrUnsafeBufferHandling`,
  `cert-err33-c` for intentionally ignored I/O return values, the reserved
  identifier check for the required `_POSIX_C_SOURCE` feature macro,
  `bugprone-easily-swappable-parameters` for small C APIs that intentionally
  take adjacent name/notes/row/column values, and related CERT aliases. No
  broad “ignore all” suppression is used.
- One C23 false positive is also excluded: `clang-analyzer-valist.Uninitialized`
  does not recognize C23's `__builtin_c23_va_start` when a `va_list` is passed
  to a forwarding helper.
- Added `scripts/check-static.sh`, which configures a compile-commands database
  (`$TMPDIR/apl1-tidy-build` by default) and analyzes production `src/*.c`
  files. It is not a build dependency; the GitHub workflow runs it as a
  separate step.

## Verification record (2026-11-24)

- Baseline run: 11 production C files analyzed, zero findings.
- A deliberate `memset(buf, sizeof(buf), 0)` probe produced a
  `bugprone-suspicious-memset-usage` finding and exit 1; restoring the file
  returned the check to zero findings.
- Compiler format-string diagnostics remain enabled by the normal build; the
  analyzer covers the same call sites.

## Dependencies / boundaries

- No code feature dependency.
- This task establishes a baseline; substantive findings should be fixed in small follow-up changes.
