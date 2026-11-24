# tests/tui/

TUI-specific tests that link Notcurses modules but must never require an
interactive terminal. Layout geometry, navigation/tabs, menu state, focus,
list viewport, dialog/forms/choices, status rendering, command-line state,
command parsing, action mapping, theme roles, text helpers, and frame safety
are automated here; anything needing a real terminal (frame rendering, mouse
input, resize, limited-color behavior) is covered by the opt-in smoke test and
the relevant work item.

The `apl1_tui_tests` binary is terminal-free. It does not call
`notcurses_init`; color/capability behavior is tested through the pure theme
role model.
