# src/db/

SQLite persistence boundary: connection lifecycle, database-path resolution,
schema migrations, and repositories. This is the only area of the project that
executes SQL.

Rules:

- Translates SQLite failures into the application error type from `src/core`.
- Never calls Notcurses or prints to the terminal.
- Uses parameterized SQL and transactions for related writes.
