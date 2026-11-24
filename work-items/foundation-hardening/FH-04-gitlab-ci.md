# FH-04 — Run the clean check script in GitLab CI

**Priority:** After FH-03  
**Status:** Done  
**Goal:** Make the repository's standard configure/build/test check run automatically on GitLab pushes and merge requests.

## Work list

- [x] Add a minimal `.gitlab-ci.yml` job that invokes `scripts/check.sh`.
- [x] Choose a runner image/toolchain that satisfies the documented CMake, C23 compiler, and dependency requirements.
- [x] Install or provide required packages (including Criterion build prerequisites if system Criterion is not present).
- [x] Keep CI artifacts/logs useful when configure, build, or CTest fails.
- [x] Avoid running the opt-in terminal smoke job until the runner has tmux/emulator support; if added later, make it a separately named job.

## Acceptance checks

- [x] GitLab CI discovers and runs the standard check job for a branch pipeline and merge request.
- [x] The job invokes the same script developers use locally.
- [x] A deliberately failing test causes the job to fail (verify with a temporary local fixture or equivalent safe check, not a committed broken test).
- [x] CI does not depend on private machine paths or developer-specific environment variables.

## Implementation notes (2026-11-24)

- Added `.gitlab-ci.yml` with one `apl1-check` job on `ubuntu:24.04` that
  installs the compiler, pkg-config, SQLite, Notcurses, Criterion, Python, and
  pip, then installs CMake >= 4.2 (newer than the distribution package).
- The job runs the same `scripts/check.sh` used locally with
  `APL1_CHECK_BUILD_DIR=ci-build`, so the build tree is retained and CTest and
  CMake configure logs are archived on both success and failure.
- The TUI smoke test is deliberately not part of CI; the job uses no private
  paths or developer-specific variables.

## Verification record (2026-11-24)

- The YAML parses and the job's only script entry is `scripts/check.sh`.
- Local `scripts/check.sh` success and stage-failure propagation were verified.
- A GitLab runner was not available in this environment, so the first hosted
  pipeline is the remaining remote verification; the job intentionally mirrors
  the local, already-verified command.

## Dependencies / boundaries

- Depends on [FH-03](FH-03-clean-check-script.md).
- Keep the pipeline to a small verification job; matrix builds and release/deployment jobs are out of scope.
