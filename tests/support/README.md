# tests/support/

Test-only helpers shared by the Criterion executables. They build as the
`apl_test_support` static library and are never linked by production targets.

`test_support.h` provides temporary-directory creation and recursive cleanup for
SQLite-backed tests. Directories are created beneath a valid absolute `TMPDIR`,
falling back to `/tmp` when `TMPDIR` is unset or unusable; each test gets a
unique directory that is removed completely afterwards without following
symlinks outside it.
