# tests/tui/

TUI-specific tests that link Notcurses modules but must never require an
interactive terminal. Only contracts that do not call `notcurses_init`
(for example, safe shutdown of an uninitialized handle) are automated here;
anything needing a real terminal is manual-smoke-checked and noted in the
relevant work item.
