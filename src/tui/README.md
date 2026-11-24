# src/tui/

Notcurses presentation: TUI lifecycle, screens, tables/forms, and input handling.
This is the only area of the project that initializes Notcurses.

Rules:

- Calls application/domain/db use cases; never bypasses them with direct SQL.
- Keeps drawing/input logic out of the lifecycle module.
