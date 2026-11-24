# src/core/

Shared foundation types with no dependencies on other project modules, SQLite, or
Notcurses. Everything here must be includable by `src/app`, `src/db`, `src/domain`,
and `src/tui` without pulling in a layer's headers.

First resident (1.1): the application error type (`apl_error`/`apl_status`).
