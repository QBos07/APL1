# FH-07 — Exercise the ASan/UBSan test build

**Priority:** Before substantial Phase 2 domain code  
**Status:** Not started  
**Goal:** Validate the existing CMake sanitizer options against the current tests and establish a repeatable command for later regression checks.

## Work list

- [ ] Configure a fresh build with both `ENABLE_ASAN=ON` and `ENABLE_UBSAN=ON`.
- [ ] Build all targets and run the unit/integration and TUI lifecycle CTest suites.
- [ ] Record compiler/runtime versions and any sanitizer/platform limitations in the work-item verification notes.
- [ ] Fix defects found in separate focused changes; do not suppress a sanitizer report just to make this task pass.
- [ ] Add the successful sanitizer command to CONTRIBUTING or the check-script documentation as a separate optional workflow.

## Acceptance checks

- [ ] The combined ASan/UBSan configure, build, and test commands pass on a supported compiler/runtime.
- [ ] Any environment-specific failure is documented and converted into a concrete follow-up rather than reported as a pass.
- [ ] The normal unsanitized build remains unchanged.
- [ ] Sanitizers are not forced on by default for all developers or CI jobs.

## Dependencies / boundaries

- Depends on the basic test suite; may follow [FH-03](FH-03-clean-check-script.md).
- Does not introduce a coverage threshold or fuzzing framework.
