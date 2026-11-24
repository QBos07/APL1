# FH-01 — Check in a reproducible TUI smoke test

**Priority:** Before or at the start of Phase 2  
**Status:** Done  
**Goal:** Replace the temporary `/tmp` pty experiment with a repeatable repository script that exercises the actual app in a terminal-like session.

## Work list

- [x] Check in `scripts/smoke_tui.py` and document its Python/runtime dependencies.
- [x] Use `tmux` as the pty/terminal driver if it is available in the supported environment; otherwise select a fuller maintained terminal emulator. Do not keep adding one-off DA1/CPR/OSC/XTGETTCAP/kitty/palette response cases to the current hand-rolled responder.
- [x] Let the script accept an application/build path and a timeout so it works from a developer build and CI.
- [x] Run the app in a fresh isolated database location and an isolated terminal session.
- [x] Assert initial content, resize/redraw, clean exit, and terminal/session cleanup. Fail with a useful diagnostic and preserve optional raw output on request.
- [x] Keep generated logs in a temporary directory by default; make an explicit output path available for debugging rather than checking machine-specific logs into Git.
- [x] Add a short README/CONTRIBUTING command showing how to run it locally.

## Acceptance checks

- [x] Another contributor can run one documented command and reproduce the 1.9 startup/resize/quit checks.
- [x] The test does not depend on an untracked script or file under `/tmp`.
- [x] The harness uses a real terminal driver/emulator and does not depend on a growing custom terminal-protocol responder.
- [x] The test detects timeout, nonzero app exit, missing shell content, failed resize behavior, and failure to tear down its session.
- [x] The application uses a temporary database and leaves the user's real database untouched.
- [x] Failure output identifies the failed assertion and provides a path to captured terminal output when requested.

## Implementation notes (2026-11-24)

- Added `scripts/smoke_tui.py` (Python 3 standard library only). It starts a
  private Xvfb display, runs the built app in xterm with a unique window title,
  and uses xdotool only for the window resize and the quit key.
- xterm on Xvfb is the selected terminal driver. A detached tmux (or screen)
  server does not answer Notcurses' OSC palette interrogation, so
  `notcurses_init` blocks before drawing; xterm is a full emulator and answers
  the queries deterministically.
- Each run uses a fresh temporary database, a private `XDG_DATA_HOME`/`HOME`,
  and a unique xterm title. It asserts startup content, a redraw after resize,
  app exit status 0, xterm window cleanup, schema version 1 in the chosen
  database, and that the environment-derived database was not created.
- `--app`, `--timeout`, `--output`, and `--keep` are supported. Logs stay in a
  temporary directory by default, are kept on failure, and can be written to an
  explicit path.

## Verification record (2026-11-24)

- `scripts/smoke_tui.py --app /tmp/apl1-hardening-build/apl1 --output /tmp/apl1-smoke-out.log`:
  passed every assertion; repeated 5/5 successful runs.
- The CTest wrapper (`apl1_tui_smoke`) passed with the opt-in configure below.
- Dependencies are documented in README/CONTRIBUTING (Python 3, Xvfb, xterm,
  xdotool). No untracked `/tmp` script or hand-rolled protocol responder is
  used.

## Dependencies / boundaries

- Depends on the existing 1.9 TUI shell and the chosen supported Notcurses build.
- Optional CTest registration is separate: [FH-02](FH-02-ctest-tui-smoke.md).
- Do not turn this into a full terminal compatibility suite.
