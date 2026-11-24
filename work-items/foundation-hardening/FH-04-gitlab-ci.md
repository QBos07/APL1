# FH-04 — Run the clean check script in GitLab CI

**Priority:** After FH-03  
**Status:** Not started  
**Goal:** Make the repository's standard configure/build/test check run automatically on GitLab pushes and merge requests.

## Work list

- [ ] Add a minimal `.gitlab-ci.yml` job that invokes `scripts/check.sh`.
- [ ] Choose a runner image/toolchain that satisfies the documented CMake, C23 compiler, and dependency requirements.
- [ ] Install or provide required packages (including Criterion build prerequisites if system Criterion is not present).
- [ ] Keep CI artifacts/logs useful when configure, build, or CTest fails.
- [ ] Avoid running the opt-in terminal smoke job until the runner has tmux/emulator support; if added later, make it a separately named job.

## Acceptance checks

- [ ] GitLab CI discovers and runs the standard check job for a branch pipeline and merge request.
- [ ] The job invokes the same script developers use locally.
- [ ] A deliberately failing test causes the job to fail (verify with a temporary local fixture or equivalent safe check, not a committed broken test).
- [ ] CI does not depend on private machine paths or developer-specific environment variables.

## Dependencies / boundaries

- Depends on [FH-03](FH-03-clean-check-script.md).
- Keep the pipeline to a small verification job; matrix builds and release/deployment jobs are out of scope.
