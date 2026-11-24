# FH-03 — Add a clean configure/build/test script

**Priority:** Before Phase 2  
**Status:** Done  
**Goal:** Give every implementation cycle one repeatable command that configures a fresh build, compiles all normal targets, and runs CTest.

## Work list

- [x] Add executable `scripts/check.sh` with strict shell error handling.
- [x] Use a fresh temporary build directory by default and remove only that directory on exit; never delete or reuse the developer's existing build tree implicitly.
- [x] Run CMake configure with testing enabled, build all targets, and run `ctest --output-on-failure`.
- [x] Preserve command failures/exit status and print which stage failed.
- [x] Allow documented environment/configuration overrides for build directory or CMake arguments without making cleanup destructive.
- [x] Document the command in README and CONTRIBUTING; keep manual commands available for incremental development.

## Acceptance checks

- [x] One command from the repository root performs configure, build, and CTest from a clean state.
- [x] A failing configure/build/test produces a nonzero script exit code and identifies the failed stage.
- [x] The script does not remove or overwrite `cmake-build-debug`, `build`, or another pre-existing directory by default.
- [x] A successful run produces the same result as the documented manual CMake/CTest workflow.

## Implementation notes (2026-11-24)

- Added executable `scripts/check.sh` with Bash strict mode
  (`set -euo pipefail`).
- By default it creates a fresh `mktemp -d` directory beneath `TMPDIR` and a
  trap removes only that directory. `APL1_CHECK_BUILD_DIR` opts into a specific
  build directory, which is never removed; `APL1_CHECK_CMAKE_ARGS` adds simple
  configure flags.
- Configure always enables testing, then builds all targets and runs
  `ctest --output-on-failure`. Each stage is named; a failure prints the stage
  and exits with the command's status.

## Verification record (2026-11-24)

- `scripts/check.sh` completed configure, build, and CTest successfully and
  removed its temporary directory.
- Reusing `APL1_CHECK_BUILD_DIR=/tmp/apl1-hardening-build` passed and left the
  directory intact.
- A deliberately broken configure (`APL1_CHECK_CMAKE_ARGS=-DCMAKE_C_COMPILER=/bin/false`)
  exited nonzero with `check: Configure failed (exit 1)`.
- The script does not enable sanitizers or the opt-in TUI smoke test.

## Dependencies / boundaries

- Can run with the existing test suite; does not depend on domain Phase 2 work.
- Does not enable sanitizers or the opt-in TUI smoke test by default.
