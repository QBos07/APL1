# FH-11 — Map application failures to distinct process exit codes

**Priority:** Before scripting/CLI use expands  
**Status:** Done  
**Goal:** Let scripts distinguish successful exit, invalid invocation, terminal failure, and application/database failure without parsing stderr text.

## Work list

- [x] Define a small stable process-exit-code mapping in one place.
- [x] Keep success at zero and distinguish usage/configuration, TUI/terminal, and application/database/runtime failure categories.
- [x] Map `apl_status` values to process codes in `main` or a small testable helper; do not return arbitrary SQLite/Notcurses library codes directly.
- [x] Print the existing useful human-readable error message independently of the numeric exit code.
- [x] Add unit tests for status-to-exit mapping and a process-level smoke check for representative success/failure paths.
- [x] Document exit-code meanings for shell/script users.

## Acceptance checks

- [x] Each documented failure class exits nonzero with its documented distinct code.
- [x] Success still exits zero.
- [x] Exit mapping tests do not require a terminal.
- [x] Error text remains actionable and is not used as the only machine-readable signal.

## Implementation notes (2026-11-24)

- Added `src/app/exit_code.{h,c}` with a stable mapping: 0 success,
  1 application/database/runtime failure, 2 invalid invocation or configuration,
  3 terminal/TUI failure. `APL_ERR_INVALID_ARGUMENT` and `APL_ERR_NOT_FOUND` map
  to usage; TUI init/render/input map to terminal; all other/unknown codes map
  to the application failure code.
- `main` returns the mapped code on every failure path while still printing the
  existing human-readable message to stderr.
- README documents the codes for shell/script users.

## Verification record (2026-11-24)

- Unit tests cover every status family and an unknown status.
- Process-level tests use the built binary: `--help` exits 0, unknown option and
  missing database value exit 2, and a no-terminal launch exits 3 without
  creating the database file.
- Mapping tests require no terminal.

## Dependencies / boundaries

- Depends on [1.1](../phase-1/1.1-application-error-type.md) and the current `main` composition.
- Does not add headless application commands.
