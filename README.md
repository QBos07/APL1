# Formula 1 Team Management Database

A local terminal application for keeping detailed records about one or more Formula 1 teams. It uses SQLite for persistent data and Notcurses for its text user interface.

This is a **user-maintained database**, not a race simulator. The user enters team information, finances, development work, parts changes, events, and driving outcomes. The application helps organize, search, review, and summarize those records; it does not generate schedules or results, fetch live data, or fill in missing facts.

## Product experience

- Switch between multiple team workspaces and keep each team's records separate.
- Use conventional menus, forms, and tables, or enter concise commands that open the same guided actions.
- Track money with a transaction history, opening balance, budget plans, and actual-versus-plan reports.
- Keep development logs, part/component lifecycle history, manually entered event outcomes, and an audit trail.
- Search and filter records, inspect related history, and see clearly labeled summaries derived from saved data.

The product requirements define the intended behavior and acceptance criteria: [REQUIREMENTS.md](REQUIREMENTS.md).

## Project status

Phase 2 is implemented: the TUI has a modern shell with a window/menu bar,
vertical view tabs, a persistent bottom status bar, a bottom-rail team switcher,
interactive form/choice dialogs, and an ex-style command line. You can create
teams, edit names/notes, archive teams, switch the active team, and reopen the
database with the selected team still active. Navigation and team actions are
available from menus, tabs, dialogs, mouse clicks, and typed commands that use
the same use cases. Other sections currently show an honest placeholder until
their phase lands. The remaining work is divided into small slices in
[WORK_BREAKDOWN.md](WORK_BREAKDOWN.md).

## Build and test

### Requirements

- CMake 4.2 or newer
- A C23-capable C compiler
- Notcurses Core and SQLite 3 for the application
- Criterion for the test target
- Git/network access on first configure if CMake needs to fetch a dependency not installed on the system

The CMake project prefers system Notcurses, SQLite, and Criterion installations where available. SQLite and Notcurses have fetch/build fallbacks. If Criterion is unavailable, the test dependency setup also requires Meson and Ninja.

### Configure and build

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
```

### Run tests

```sh
ctest --test-dir build --output-on-failure
```

### Clean check

One command configures a fresh temporary build directory, builds all targets,
and runs CTest:

```sh
./scripts/check.sh
```

Set `APL1_CHECK_BUILD_DIR=build` to reuse a specific build directory (it is
kept, not deleted), or `APL1_CHECK_CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Debug"` to add
configure arguments. Manual incremental CMake/CTest commands remain available.

### TUI smoke test (opt-in)

The interactive smoke check runs the real application in xterm on a private
Xvfb display. It exercises the window-bar dropdowns (keyboard and mouse), tab
switching, dialog-based team creation, the rail/status team switcher, command
cancel/navigation/team actions, resizing with an open menu/dialog, quit and
relaunch persistence, and a limited-color (`xterm-mono`) launch. It requires
Python 3, `Xvfb`, `xterm`, and `xdotool`:

```sh
./scripts/smoke_tui.py --app build/apl1
```

Use `--output /tmp/apl1-smoke.log` to keep the harness log and raw terminal
output, `--timeout` to bound each step, or `--keep` to keep the temporary
database and logs. The script uses a fresh temporary database beneath a private
`XDG_DATA_HOME` and never touches the user's real database.

The same check can be registered with CTest, off by default:

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DAPL1_ENABLE_TUI_SMOKE=ON
ctest --test-dir build -R apl1_tui_smoke --output-on-failure
```

The regular test suite stays headless and does not require Xvfb, xterm, or
xdotool.

### Continuous integration

`.github/workflows/ci.yml` runs the formatting check, clang-tidy baseline, and
`scripts/check.sh` with the opt-in TUI smoke test enabled on `ubuntu-26.04`.
It installs the documented toolchain and uploads build/test/smoke logs as
artifacts.

Run the application from an interactive terminal with:

```sh
./build/apl1
```

The first launch shows a welcome screen with no seeded teams. Press `n` to
create a team; `Tab` moves between the name and notes fields, `Enter` saves, and
`Esc` cancels. In the team list, use the arrow keys to select, `Enter` to open,
`s` to make the selected team active, `a` to show archived teams, and `q` to
quit. Team details support editing (`e`) and archiving (`x`, with confirmation).
`Esc` returns to the previous screen. A launch without a usable interactive
terminal (e.g. `apl1 < /dev/null`) fails cleanly with a message on stderr,
creates no database file, and exits with the terminal-failure code.

### Command-line options

```text
apl1 [--database <path>] [--help]
```

- `--database <path>` uses exactly that SQLite file after the terminal has been
  verified usable. It overrides the environment-derived default and does not
  create missing parent directories.
- `-h`, `--help` prints usage and exits successfully without requiring a
  terminal or opening a database.
- Unknown options, missing option values, and unexpected positional arguments
  print a usage error and exit with the usage code.

### Exit codes

| Code | Meaning |
| ---- | ------- |
| `0` | Success |
| `1` | Application, database, or runtime failure |
| `2` | Invalid invocation or configuration (usage) |
| `3` | Terminal/TUI failure |

Scripts should use these numbers instead of parsing the human-readable stderr
message.

### Database location

The database path is resolved in this order (implemented in `src/db/path.h`):

1. An explicitly configured path takes precedence.
2. `$XDG_DATA_HOME/apl1/apl1.sqlite3` when `XDG_DATA_HOME` is set and absolute.
3. `$HOME/.local/share/apl1/apl1.sqlite3` when `HOME` is set and absolute.

Missing parent directories of an environment-derived default are created on
first run. When no usable location is configured, the application fails with a
clear error instead of creating a database in the current working directory.

The database is created during application startup, after the terminal has
been verified usable (work item 1.10): a launch without an interactive
terminal never creates a database file.

## Design boundaries

- SQLite is the local source of truth; normal operation is offline.
- Menu actions and commands must share the same business logic and validation.
- User-entered records are distinct from derived totals and reports.
- Important changes must remain reviewable; posted financial records are corrected in a traceable way.
- No sample team or official championship data is inserted unless explicitly offered as an opt-in sample.

## Contributing / implementation workflow

Code conventions for this project are documented in [CONTRIBUTING.md](CONTRIBUTING.md).

Take one unchecked item at a time from [WORK_BREAKDOWN.md](WORK_BREAKDOWN.md). Keep each change small, add or update tests with business logic, and update the checklist only when the item is implemented and verified. Later phases intentionally remain less detailed so their work can be split based on what is learned from the earlier slices.
