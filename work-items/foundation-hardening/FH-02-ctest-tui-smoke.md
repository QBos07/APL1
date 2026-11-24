# FH-02 — Register the TUI smoke test as opt-in CTest

**Priority:** After FH-01  
**Status:** Not started  
**Goal:** Make the interactive smoke test available to automated runs without making normal unit/integration tests require a terminal emulator.

## Work list

- [ ] Add an OFF-by-default CMake option for the terminal smoke test.
- [ ] When enabled, find/validate Python and the selected terminal driver (`tmux` or chosen emulator) during configuration and give a clear missing-dependency error.
- [ ] Register `scripts/smoke_tui.py` as a CTest test with a bounded timeout.
- [ ] Keep the existing unit/integration and TUI lifecycle tests runnable with the option disabled and without an interactive terminal.
- [ ] Document the opt-in configure and CTest commands.

## Acceptance checks

- [ ] Default CMake configuration does not require Python/tmux and registers no terminal smoke test.
- [ ] Enabling the option registers one clearly named smoke test.
- [ ] Running that CTest test passes on a supported terminal-driver environment and reports a useful failure otherwise.
- [ ] The regular `ctest` path remains headless and does not start tmux.

## Dependencies / boundaries

- Depends on [FH-01](FH-01-reproducible-tui-smoke.md).
- Do not make CI run an interactive test until its runner image has the required terminal driver.
