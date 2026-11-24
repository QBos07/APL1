# tests/tui/

TUI-specific tests that link Notcurses modules but must never require an
interactive terminal. Layout geometry, navigation, list viewport, dialog state,
frame safety, and contracts that do not call `notcurses_init` are automated here;
anything needing a real terminal (frame rendering, mouse input, resize) is
covered by the opt-in smoke test and the relevant work item.
