# FH-01 — Check in a reproducible TUI smoke test

**Priority:** Before or at the start of Phase 2  
**Status:** Not started  
**Goal:** Replace the temporary `/tmp` pty experiment with a repeatable repository script that exercises the actual app in a terminal-like session.

## Work list

- [ ] Check in `scripts/smoke_tui.py` and document its Python/runtime dependencies.
- [ ] Use `tmux` as the pty/terminal driver if it is available in the supported environment; otherwise select a fuller maintained terminal emulator. Do not keep adding one-off DA1/CPR/OSC/XTGETTCAP/kitty/palette response cases to the current hand-rolled responder.
- [ ] Let the script accept an application/build path and a timeout so it works from a developer build and CI.
- [ ] Run the app in a fresh isolated database location and an isolated tmux session.
- [ ] Assert initial content, resize/redraw, clean exit, and terminal/session cleanup. Fail with a useful diagnostic and preserve optional raw output on request.
- [ ] Keep generated logs in a temporary directory by default; make an explicit output path available for debugging rather than checking machine-specific logs into Git.
- [ ] Add a short README/CONTRIBUTING command showing how to run it locally.

## Acceptance checks

- [ ] Another contributor can run one documented command and reproduce the 1.9 startup/resize/quit checks.
- [ ] The test does not depend on an untracked script or file under `/tmp`.
- [ ] The harness uses a real terminal driver/emulator and does not depend on a growing custom terminal-protocol responder.
- [ ] The test detects timeout, nonzero app exit, missing shell content, failed resize behavior, and failure to tear down its session.
- [ ] The application uses a temporary database and leaves the user's real database untouched.
- [ ] Failure output identifies the failed assertion and provides a path to captured terminal output when requested.

## Dependencies / boundaries

- Depends on the existing 1.9 TUI shell and the chosen supported Notcurses build.
- Optional CTest registration is separate: [FH-02](FH-02-ctest-tui-smoke.md).
- Do not turn this into a full terminal compatibility suite.
