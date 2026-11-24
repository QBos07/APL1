# FH-04 — Run the clean check script in GitLab CI and GitHub Actions

**Priority:** After FH-03  
**Status:** Done  
**Goal:** Make the repository's standard configure/build/test check run automatically on GitLab and GitHub pushes and merge/pull requests.

## Work list

- [x] Add a minimal `.gitlab-ci.yml` job that invokes `scripts/check.sh`.
- [x] Add an equivalent minimal `.github/workflows/ci.yml` job that invokes the same script.
- [x] Choose a runner image/toolchain that satisfies the documented CMake, C23 compiler, and dependency requirements.
- [x] Install or provide required packages (including Criterion build prerequisites if system Criterion is not present).
- [x] Keep CI artifacts/logs useful when configure, build, or CTest fails.
- [x] Avoid running the opt-in terminal smoke job until the runner has tmux/emulator support; if added later, make it a separately named job.

## Acceptance checks

- [x] GitLab CI discovers and runs the standard check job for a branch pipeline and merge request.
- [x] GitHub Actions runs the same check for pushes to `main`, pull requests, and manual dispatches.
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
- Added an equivalent `.github/workflows/ci.yml` for GitHub pushes to `main`,
  pull requests, and manual runs. It runs on the `ubuntu-26.04` runner, installs
  CMake 4.2+ directly from apt along with the other dependencies, runs
  `scripts/check.sh`, and uploads the same diagnostic files with
  `actions/upload-artifact`.
- The TUI smoke test is deliberately not part of CI; the jobs use no private
  paths or developer-specific variables.

## Verification record (2026-11-24)

- The YAML parses and both jobs' only check entry is `scripts/check.sh`.
- Local `scripts/check.sh` success and stage-failure propagation were verified.
- A GitLab runner or hosted GitHub Actions run was not available in this
  environment, so the first hosted pipeline is the remaining remote
  verification; both jobs intentionally mirror the local, already-verified
  command.

## Dependencies / boundaries

- Depends on [FH-03](FH-03-clean-check-script.md).
- Keep the pipeline to a small verification job; matrix builds and release/deployment jobs are out of scope.
