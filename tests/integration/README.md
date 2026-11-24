# tests/integration/

SQLite-backed tests (Criterion). Each test creates its own temporary database
under a private directory and removes it afterwards; tests never touch the
developer's real database or change process-global environment variables.
