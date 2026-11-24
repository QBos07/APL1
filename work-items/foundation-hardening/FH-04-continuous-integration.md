# FH-04 — Run the hardening checks in GitHub Actions

**Priority:** After FH-03  
**Status:** Done  
**Goal:** Make the repository's formatting, static-analysis, configure/build/test, and TUI smoke checks run automatically on GitHub pushes, pull requests, and manual dispatches.

## Work list

- [x] Add a minimal `.github/workflows/ci.yml` job.
- [x] Choose a runner image/toolchain that satisfies the documented CMake, C23 compiler, and dependency requirements.
- [x] Install or provide required packages (including Criterion build prerequisites if system Criterion is not present).
- [x] Run the formatting check, static analysis, and `scripts/check.sh`.
- [x] Enable the opt-in TUI smoke test on the CI runner and run it headlessly with Xvfb.
- [x] Keep CI artifacts/logs useful when formatting, analysis, configure, build, or CTest fails.
- [x] Remove the former GitLab pipeline so there is one CI definition.

## Acceptance checks

- [x] GitHub Actions runs the workflow for pushes to `main`, pull requests, and manual dispatches.
- [x] Formatting, static analysis, build, and tests use the same scripts developers use locally.
- [x] The TUI smoke test runs in CI through the existing opt-in CTest registration.
- [x] A failing step fails the job; local stage-failure propagation was verified.
- [x] CI does not depend on private machine paths or developer-specific environment variables.
- [x] Failure logs are uploaded as workflow artifacts.

## Implementation notes (2026-11-24)

- The workflow runs on the `ubuntu-26.04` runner and installs CMake 4.2+
  directly from apt, along with the compiler, pkg-config, SQLite, Notcurses,
  Criterion, clang-format, clang-tidy, Python, Xvfb, xterm, xdotool, and
  `xfonts-base`.
- `scripts/check-format.sh` and `scripts/check-static.sh` run as dedicated
  steps before the build.
- `scripts/check.sh` runs with `APL1_CHECK_BUILD_DIR=ci-build` and
  `APL1_CHECK_CMAKE_ARGS=-DAPL1_ENABLE_TUI_SMOKE=ON`, so CTest also runs
  `apl1_tui_smoke`. The smoke harness starts its own Xvfb display and xterm
  window; no interactive runner is required.
- The smoke test writes `ci-build/apl1-tui-smoke.log`. CTest and CMake configure
  logs plus that file are uploaded with `actions/upload-artifact` on every run.
- GitLab CI was removed in favor of the single GitHub Actions workflow.

## Verification record (2026-11-24)

- The workflow YAML parses.
- `scripts/check-format.sh`, `scripts/check-static.sh`, and `scripts/check.sh`
  pass locally; the opt-in `apl1_tui_smoke` CTest test passes with the same
  arguments and writes the log used as a CI artifact.
- Local script failures propagate nonzero exit status (verified for configure
  and static-analysis probes).
- A hosted GitHub Actions run was not available in this environment, so the
  first hosted workflow run remains the remote verification.

## Dependencies / boundaries

- Depends on [FH-03](FH-03-clean-check-script.md), [FH-08](FH-08-formatting-check.md), [FH-09](FH-09-static-analysis-baseline.md), and [FH-02](FH-02-ctest-tui-smoke.md).
- Keep the pipeline to one small verification job; matrix builds and release/deployment jobs are out of scope.
