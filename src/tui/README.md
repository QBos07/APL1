# src/tui/

Notcurses presentation: TUI lifecycle, screens, tables/forms, and input handling.
This is the only area of the project that initializes Notcurses.

Rules:

- Calls application/domain/db use cases; never bypasses them with direct SQL.
- Keeps drawing/input logic out of the lifecycle module.
- Layout geometry is pure (`layout.h`): it computes region rectangles without a
  Notcurses context so it can be unit-tested. The shared frame owns all child
  planes; screens borrow a region only for one draw. The standard plane is the
  base surface except for the "terminal too small" notice. See
  [2.0.1](../../work-items/phase-2/2.0.1-tui-layout-resize-decision.md).
- Focus is semantic screen state. Mouse clicks resolve through the frame geometry
  and invoke the same commands as the keyboard; see
  [2.0.2](../../work-items/phase-2/2.0.2-mouse-focus-decision.md).
- `navigation.c`, `list.c`, and `dialog.c` hold terminal-free state models;
  `frame.c` owns planes and drawing helpers; `shell.c` composes the shared frame
  with the Phase 2 team screens and app use cases.
