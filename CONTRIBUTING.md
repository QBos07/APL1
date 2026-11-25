# Contributing / code conventions

Small-project C conventions matching the existing C23/CMake setup. They cover
the conventions already used by the code and the optional formatting/static
checks added during foundation hardening.

## Build, language, and warnings

- C23 with extensions disabled (`CMAKE_C_STANDARD 23`, `CMAKE_C_EXTENSIONS NO`) and
  strict diagnostics as errors: `-Wall -Wextra -Wpedantic -Werror
  -pedantic-errors` plus `-Wcast-qual -Wformat-security -Wmissing-declarations
  -Wmissing-prototypes -Wold-style-definition -Wpointer-arith -Wshadow
  -Wstrict-prototypes -Wundef -Wvla`.
- Configure/build/test with the commands in the README
  (`cmake -S . -B build -DBUILD_TESTING=ON`, `cmake --build build`,
  `ctest --test-dir build --output-on-failure`).
- CMake target source lists are explicit: add new files to the target that needs
  them instead of globbing directories.
- Files that call POSIX APIs (temporary directories, `mkdir`, etc.) define
  `_POSIX_C_SOURCE 200809L` at the top of the file, before any include.

## C23 language usage

- Use the C23 keywords directly: `bool`, `true`, and `false`. Do not include
  `<stdbool.h>`.
- Use `nullptr` for null pointer constants instead of `NULL`.
- Use the C23 one-argument form of `va_start(args)`; its second parameter is
  obsolete.
- Use `{}` for zero initialization instead of `{0}`.
- Declare compile-time integer constants with `static constexpr` (for example
  `APL_DB_PATH_MAX`) so file-scope constants have internal linkage in every
  translation unit that includes their header. Use `const` for everything that
  is not mutated: local variables, pointers that are not reseated (`T *const`),
  and pointees that are only read (`const T *`).
- Omit the top-level `const` qualifier from parameters in function prototypes
  (`void f(T value)`, `void f(T *pointer)`): C ignores it for the function type
  and it only describes the callee's local copy. Definitions keep those
  qualifiers for local immutability; pointee const (`const T *`) is part of the
  interface and stays in both.
- Use standard C23 attributes where they apply, for example
  `[[gnu::format(printf, 3, 4)]]` for printf-like functions and
  `[[maybe_unused]]` for intentionally unused parameters.

## File, header, and type naming

- Modules live in the directories confirmed in
  [0.3](work-items/phase-0/0.3-source-layout.md): `src/core` (shared foundation),
  `src/app` (application coordination), `src/db` (SQLite), `src/domain` (business
  rules), `src/tui` (Notcurses). A module's `.c` and its public `.h` live together
  in that module directory, and the header name matches the file: for example
  `src/db/connection.h`/`connection.c`.
- Include guards are the header name uppercased with the module prefix and an `_H`
  suffix, for example `APL_DB_CONNECTION_H` in `src/db/connection.h`.
- All public identifiers are project-prefixed and module-prefixed in lowercase
  snake case: `apl_status`, `apl_error`, `apl_db_open`, `apl_tui_stop`. Internal
  helpers are `static` with the same naming style.
- Public types are small, caller-owned structs with documented ownership; no
  hidden global state.

## Errors

- Application functions report failures by returning an `apl_status` code
  (`APL_OK` on success) and, where details matter, filling a caller-supplied
  `apl_error *` (see `src/core/error.h`). No global "last error" variable.
- SQLite and Notcurses errors are translated at the `src/db` / `src/tui`
  boundaries into that error type.

## Tests

- Unit tests use [Criterion](https://criterion.readthedocs.io/) and live in
  `tests/unit/test_<module>.c` (no database, no terminal). SQLite-backed
  integration tests live in `tests/integration/` and use a fresh temporary
  database per test; TUI tests live in `tests/tui/` and must not require an
  interactive Notcurses terminal.
- Shared test-only helpers (temporary directories, database fixtures) live in
  `tests/support/` and build as the `apl_test_support` target. Only test
  executables link that target; production libraries must not expose or depend
  on headers under `tests/`.
- `Test` suites are named after the module under test and test names describe the
  behavior, for example `Test(error, set_failure_captures_message)`.
- All business-logic and integration tests compile into the single test
  executable `apl1_tests`, registered with CTest as `apl1_unit_tests` (renamed in
  work item 0.2). TUI state/lifecycle tests compile into a separate
  `apl1_tui_tests` executable (CTest `apl1_tui_tests`), which is the only test
  target that links Notcurses modules, and its tests must not require an
  interactive terminal.
  Business logic tests must not require an interactive Notcurses terminal.
- Run one suite during development with
  `ctest --test-dir build --output-on-failure -R apl1_unit_tests` or by running
  the `apl1_tests` binary directly, which supports Criterion's filters.
- `scripts/check.sh` is the one-command clean check (fresh temporary build
  directory, configure with tests, build, CTest). It never deletes an existing
  build directory; `APL1_CHECK_BUILD_DIR` opts into reusing a specific one.
  CI runs this same script via `.github/workflows/ci.yml`.
- The interactive TUI smoke test (`scripts/smoke_tui.py`) uses xterm on a
  private Xvfb display because Notcurses interrogates the terminal palette and
  a detached tmux/screen server does not answer those queries. It is opt-in at
  configure time with `-DAPL1_ENABLE_TUI_SMOKE=ON`; the default test suite does
  not need Python, Xvfb, xterm, or xdotool.
- Sanitizer check (optional, not forced on normal builds):

  ```sh
  cmake -S . -B build-sanitize -DBUILD_TESTING=ON -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
  cmake --build build-sanitize
  ctest --test-dir build-sanitize --output-on-failure
  ```

## Formatting

- The repository uses one `.clang-format` at the project root. It targets
  clang-format 21; other major versions may report output differences, so the
  check uses whatever `clang-format` is installed.
- `scripts/check-format.sh` checks without modifying files. `scripts/format.sh`
  rewrites them. clang-format is not a normal build dependency; CI runs the
  check as a separate step, and `scripts/check.sh` does not require it.
- Keep formatting-only changes separate from behavior changes.

## Static analysis

- `scripts/check-static.sh` is an optional local check and also runs as a
  separate CI step. It analyzes production sources in `src/`, configures a
  compile-commands database (default build directory `$TMPDIR/apl1-tidy-build`),
  and uses the checked-in `.clang-tidy`. clang-tidy is not a build dependency.
- The configuration limits checks to high-signal families and excludes noisy
  standard-C false positives documented in the FH-09 work item. Findings should
  be fixed or handled with a narrow, explained exclusion rather than a broad
  “ignore everything” suppression.

## Boundaries recap

- Keep Notcurses out of business-logic tests; keep SQL inside `src/db`.
- Menus/commands must go through the same use cases; never bypass them from TUI
  code.
- Do not seed sample teams, seasons, events, results, or budgets in code or tests.
