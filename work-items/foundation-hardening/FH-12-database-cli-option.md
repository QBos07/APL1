# FH-12 — Expose the configured database path on the command line

**Priority:** Early Phase 2  
**Status:** Not started  
**Goal:** Make the existing explicit `apl_app_config.database_path` override reachable to users and smoke tests.

## Work list

- [ ] Add a small argument parser for `--database <path>` and `--help` before TUI initialization.
- [ ] Pass the selected path through `apl_app_config.database_path`; do not duplicate path-resolution logic in `main`.
- [ ] Reject missing option values and unknown options with a usage message and the usage exit code from [FH-11](FH-11-process-exit-codes.md).
- [ ] Preserve the current interactive composition: parse options, initialize TUI, then initialize the app/database, then run the UI.
- [ ] Ensure `--help` and usage errors work without an interactive terminal; ensure a valid `--database` path is never opened before successful TUI initialization in the interactive path.
- [ ] Add argument parsing tests and a process-level test proving the chosen file is the one opened.
- [ ] Document the option in README.

## Acceptance checks

- [ ] `apl1 --help` prints usage and exits successfully without requiring a terminal or creating a database.
- [ ] `apl1 --database <path>` uses exactly that path after terminal initialization.
- [ ] Missing values and unknown options produce a clear usage error and nonzero usage exit code.
- [ ] Existing environment/default path behavior remains unchanged when the option is omitted.
- [ ] No arbitrary SQL/shell or general headless command mode is introduced.

## Dependencies / boundaries

- Depends on completed [1.3](../phase-1/1.3-database-path.md), [1.10](../phase-1/1.10-application-test-seam.md), and [FH-11](FH-11-process-exit-codes.md).
- Future headless commands will need a separate startup composition; do not design or implement that mode here.
