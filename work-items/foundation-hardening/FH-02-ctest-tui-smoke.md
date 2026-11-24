# FH-02 — Register the TUI smoke test as opt-in CTest

**Priority:** After FH-01  
**Status:** Done  
**Goal:** Make the interactive smoke test available to automated runs without making normal unit/integration tests require a terminal emulator.

## Work list

- [x] Add an OFF-by-default CMake option for the terminal smoke test.
- [x] When enabled, find/validate Python and the selected terminal driver (`tmux` or chosen emulator) during configuration and give a clear missing-dependency error.
- [x] Register `scripts/smoke_tui.py` as a CTest test with a bounded timeout.
- [x] Keep the existing unit/integration and TUI lifecycle tests runnable with the option disabled and without an interactive terminal.
- [x] Document the opt-in configure and CTest commands.

## Acceptance checks

- [x] Default CMake configuration does not require Python/tmux and registers no terminal smoke test.
- [x] Enabling the option registers one clearly named smoke test.
- [x] Running that CTest test passes on a supported terminal-driver environment and reports a useful failure otherwise.
- [x] The regular `ctest` path remains headless and does not start tmux.

## Implementation notes (2026-11-24)

- Added the OFF-by-default `APL1_ENABLE_TUI_SMOKE` CMake option.
- When enabled, configure requires the Python 3 interpreter and Xvfb, xterm,
  and xdotool (`find_program(... REQUIRED)` with a clear fatal error), then
  registers one test: `apl1_tui_smoke`, with a 90-second timeout and the
  `tui;smoke` labels, running `scripts/smoke_tui.py` against
  `$<TARGET_FILE:apl1>`.
- Default configuration does not look for Python or any terminal driver.

## Verification record (2026-11-24)

- Default configure: `ctest -N` lists only `apl1_unit_tests` and
  `apl1_tui_tests`; the regular suite remains headless.
- `-DAPL1_ENABLE_TUI_SMOKE=ON` registers `apl1_tui_smoke`; it passes under
  CTest with the same assertions as the direct script run.
- The GitHub CI workflow installs Xvfb, xterm, and xdotool and enables this
  opt-in test, so the smoke check runs headlessly there as well.

## Dependencies / boundaries

- Depends on [FH-01](FH-01-reproducible-tui-smoke.md).
- Do not make CI run an interactive test until its runner image has the required terminal driver.
