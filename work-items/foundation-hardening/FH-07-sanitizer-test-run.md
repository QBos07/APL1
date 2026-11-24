# FH-07 — Exercise the ASan/UBSan test build

**Priority:** Before substantial Phase 2 domain code  
**Status:** Done  
**Goal:** Validate the existing CMake sanitizer options against the current tests and establish a repeatable command for later regression checks.

## Work list

- [x] Configure a fresh build with both `ENABLE_ASAN=ON` and `ENABLE_UBSAN=ON`.
- [x] Build all targets and run the unit/integration and TUI lifecycle CTest suites.
- [x] Record compiler/runtime versions and any sanitizer/platform limitations in the work-item verification notes.
- [x] Fix defects found in separate focused changes; do not suppress a sanitizer report just to make this task pass.
- [x] Add the successful sanitizer command to CONTRIBUTING or the check-script documentation as a separate optional workflow.

## Acceptance checks

- [x] The combined ASan/UBSan configure, build, and test commands pass on a supported compiler/runtime.
- [x] Any environment-specific failure is documented and converted into a concrete follow-up rather than reported as a pass.
- [x] The normal unsanitized build remains unchanged.
- [x] Sanitizers are not forced on by default for all developers or CI jobs.

## Implementation notes (2026-11-24)

- Configured a fresh build with both `ENABLE_ASAN=ON` and `ENABLE_UBSAN=ON`,
  built all targets, and ran both CTest suites.
- The first run found a real defect in the existing truncation test: its long
  input buffer was not NUL-terminated, so the formatter read past the stack
  buffer. Fixed the test buffer in a focused commit; no sanitizer report was
  suppressed.
- Environment: GCC 15.2.0, CMake 4.2.3, Linux 7.0.0-1016-oracle (aarch64),
  system Notcurses 3.0.17, SQLite 3.46.1, Criterion 2.4.1.
- The command is documented as an optional workflow in CONTRIBUTING; sanitizers
  remain off by default for normal builds and CI.

## Verification record (2026-11-24)

```sh
cmake -S . -B build-sanitize -DBUILD_TESTING=ON -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
cmake --build build-sanitize
ctest --test-dir build-sanitize --output-on-failure
```

- Result: 2/2 CTest tests passed (74/74 Criterion tests in `apl1_tests`,
  5/5 in `apl1_tui_tests`), no ASan/UBSan reports.
- No platform-specific sanitizer failure needed a follow-up.

## Dependencies / boundaries

- Depends on the basic test suite; may follow [FH-03](FH-03-clean-check-script.md).
- Does not introduce a coverage threshold or fuzzing framework.
