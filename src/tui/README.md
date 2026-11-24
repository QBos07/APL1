# src/tui/

Notcurses presentation: TUI lifecycle, redesigned shell, screens, tables/forms,
and input handling. This is the only area of the project that initializes
Notcurses.

Rules:

- Calls application/domain/db use cases; never bypasses them with direct SQL.
- Keeps drawing/input logic out of the lifecycle module.
- Layout geometry is pure (`layout.h`): it computes the window bar, tab rail,
  team-switcher strip, body, status bar, and dialog rectangle without a
  Notcurses context. The shared frame owns all child planes; screens borrow a
  region only for one draw. See
  [2.16](../../work-items/phase-2/2.16-redesign-layout-geometry.md).
- Focus is semantic screen state (`focus.h`). Mouse clicks resolve through the
  frame geometry and invoke the same commands as the keyboard; see
  [2.0.2](../../work-items/phase-2/2.0.2-mouse-focus-decision.md) and
  [2.19](../../work-items/phase-2/2.19-vertical-view-tabs.md).
- Terminal-free state models: `navigation.c` (view tabs), `list.c` (viewport),
  `menu.c` (window bar/dropdowns), `dialog.c` (confirm/info/form/choice),
  `status.c` (team/context/message rendering), `command_line.c` (prompt/history),
  `command.c` (registry/parser), `action.c` (typed command intents), `theme.c`
  (semantic style roles), `text.c` (UTF-8-safe helpers), and `focus.c`.
- `frame.c` owns planes and drawing helpers; `shell.c` composes the frame with
  menus, tabs, team screens, dialogs, commands, and app use cases. Each body
  view reserves its last row for contextual key hints that are also mouse-clickable; the window-bar `[Keys]`
  button opens the same keyboard/mouse help dialog and `[Quit]` exits safely.
- Long status/command-line rendering must stay UTF-8-safe and width-bounded;
  colors are never the only signal for focus/status.
