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

The repository is at the scaffold stage. The current executable and Criterion test are placeholders; the management features described in the requirements have not been implemented yet. The proposed work is divided into small, reviewable slices in [WORK_BREAKDOWN.md](WORK_BREAKDOWN.md). The checklist is a plan, not a claim that work is complete.

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

The application currently prints a placeholder message; its eventual terminal startup and database location will be documented here as those features are implemented.

## Design boundaries

- SQLite is the local source of truth; normal operation is offline.
- Menu actions and commands must share the same business logic and validation.
- User-entered records are distinct from derived totals and reports.
- Important changes must remain reviewable; posted financial records are corrected in a traceable way.
- No sample team or official championship data is inserted unless explicitly offered as an opt-in sample.

## Contributing / implementation workflow

Code conventions for this project are documented in [CONTRIBUTING.md](CONTRIBUTING.md).

Take one unchecked item at a time from [WORK_BREAKDOWN.md](WORK_BREAKDOWN.md). Keep each change small, add or update tests with business logic, and update the checklist only when the item is implemented and verified. Later phases intentionally remain less detailed so their work can be split based on what is learned from the earlier slices.
