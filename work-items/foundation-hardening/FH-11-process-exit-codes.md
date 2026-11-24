# FH-11 — Map application failures to distinct process exit codes

**Priority:** Before scripting/CLI use expands  
**Status:** Not started  
**Goal:** Let scripts distinguish successful exit, invalid invocation, terminal failure, and application/database failure without parsing stderr text.

## Work list

- [ ] Define a small stable process-exit-code mapping in one place.
- [ ] Keep success at zero and distinguish usage/configuration, TUI/terminal, and application/database/runtime failure categories.
- [ ] Map `apl_status` values to process codes in `main` or a small testable helper; do not return arbitrary SQLite/Notcurses library codes directly.
- [ ] Print the existing useful human-readable error message independently of the numeric exit code.
- [ ] Add unit tests for status-to-exit mapping and a process-level smoke check for representative success/failure paths.
- [ ] Document exit-code meanings for shell/script users.

## Acceptance checks

- [ ] Each documented failure class exits nonzero with its documented distinct code.
- [ ] Success still exits zero.
- [ ] Exit mapping tests do not require a terminal.
- [ ] Error text remains actionable and is not used as the only machine-readable signal.

## Dependencies / boundaries

- Depends on [1.1](../phase-1/1.1-application-error-type.md) and the current `main` composition.
- Does not add headless application commands.
