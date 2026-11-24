# FH-03 — Add a clean configure/build/test script

**Priority:** Before Phase 2  
**Status:** Not started  
**Goal:** Give every implementation cycle one repeatable command that configures a fresh build, compiles all normal targets, and runs CTest.

## Work list

- [ ] Add executable `scripts/check.sh` with strict shell error handling.
- [ ] Use a fresh temporary build directory by default and remove only that directory on exit; never delete or reuse the developer's existing build tree implicitly.
- [ ] Run CMake configure with testing enabled, build all targets, and run `ctest --output-on-failure`.
- [ ] Preserve command failures/exit status and print which stage failed.
- [ ] Allow documented environment/configuration overrides for build directory or CMake arguments without making cleanup destructive.
- [ ] Document the command in README and CONTRIBUTING; keep manual commands available for incremental development.

## Acceptance checks

- [ ] One command from the repository root performs configure, build, and CTest from a clean state.
- [ ] A failing configure/build/test produces a nonzero script exit code and identifies the failed stage.
- [ ] The script does not remove or overwrite `cmake-build-debug`, `build`, or another pre-existing directory by default.
- [ ] A successful run produces the same result as the documented manual CMake/CTest workflow.

## Dependencies / boundaries

- Can run with the existing test suite; does not depend on domain Phase 2 work.
- Does not enable sanitizers or the opt-in TUI smoke test by default.
