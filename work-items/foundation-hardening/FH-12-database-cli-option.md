# FH-12 — Expose the configured database path on the command line

**Priority:** Early Phase 2  
**Status:** Done  
**Goal:** Make the existing explicit `apl_app_config.database_path` override reachable to users and smoke tests.

## Work list

- [x] Add a small argument parser for `--database <path>` and `--help` before TUI initialization.
- [x] Pass the selected path through `apl_app_config.database_path`; do not duplicate path-resolution logic in `main`.
- [x] Reject missing option values and unknown options with a usage message and the usage exit code from [FH-11](FH-11-process-exit-codes.md).
- [x] Preserve the current interactive composition: parse options, initialize TUI, then initialize the app/database, then run the UI.
- [x] Ensure `--help` and usage errors work without an interactive terminal; ensure a valid `--database` path is never opened before successful TUI initialization in the interactive path.
- [x] Add argument parsing tests and a process-level test proving the chosen file is the one opened.
- [x] Document the option in README.

## Acceptance checks

- [x] `apl1 --help` prints usage and exits successfully without requiring a terminal or creating a database.
- [x] `apl1 --database <path>` uses exactly that path after terminal initialization.
- [x] Missing values and unknown options produce a clear usage error and nonzero usage exit code.
- [x] Existing environment/default path behavior remains unchanged when the option is omitted.
- [x] No arbitrary SQL/shell or general headless command mode is introduced.

## Implementation notes (2026-11-24)

- Added `src/app/args.{h,c}` parsing `--database <path>`,
  `--database=<path>`, and `--help`/`-h`. Missing/empty values, unknown
  options, and unexpected positional arguments are rejected with a clear usage
  message.
- `main` parses before TUI initialization. Help prints usage and exits without a
  terminal or database; usage errors print the message and usage text and use
  the FH-11 usage exit code.
- The selected path is passed through `apl_app_config.database_path`; path
  resolution and parent-directory policy are not duplicated in `main`. The TUI
  is still verified usable before the application opens the database.

## Verification record (2026-11-24)

- Nine parser unit tests cover defaults, both database forms, repeated options,
  help, missing/empty values, unknown options, positional arguments, and null
  output.
- Process tests cover help/usage/no-terminal behavior.
- `scripts/smoke_tui.py` passes `--database` to the real app and verifies that
  exactly that file exists with schema version 1 while the environment-derived
  database is absent.
- Omitting the option keeps the existing environment/default resolution
  unchanged.

## Dependencies / boundaries

- Depends on completed [1.3](../phase-1/1.3-database-path.md), [1.10](../phase-1/1.10-application-test-seam.md), and [FH-11](FH-11-process-exit-codes.md).
- Future headless commands will need a separate startup composition; do not design or implement that mode here.
