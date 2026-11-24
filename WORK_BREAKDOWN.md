# Implementation work breakdown

This is a deliberately small-slice implementation plan for the product in [REQUIREMENTS.md](REQUIREMENTS.md). Items are ordered by dependency, not by how impressive they look in a demo. Finish and verify a slice before taking the next one. If one checkbox becomes too large, split it before implementation; do not bundle unrelated features into one change.

The early phases are decomposed into concrete tasks. Later ideas are intentionally kept as broader follow-up buckets until the core database and workflows have taught us what is actually needed.

## Working rules

- Each unchecked checkbox should be a focused change with a visible completion condition.
- Add unit tests with business rules and SQLite integration tests with repository behavior.
- Keep Notcurses out of domain/business-logic tests.
- Avoid adding UI actions that bypass the shared application/business operations.
- Do not create seeded teams, seasons, events, results, budgets, or other purported user data.
- Update this checklist only after implementation and verification, not when work merely starts.

## Phase 0 — Confirm the scaffold

Goal: know what builds today and clean up only the minimum needed to make small feature work straightforward.

- [x] **0.1 Verify the current configure/build/test commands.** [Work item](work-items/phase-0/0.1-baseline-build-test.md)
- [x] **0.2 Rename the test target and CTest test.** [Work item](work-items/phase-0/0.2-test-target-names.md)
- [x] **0.3 Confirm the initial source layout.** [Work item](work-items/phase-0/0.3-source-layout.md)
- [x] **0.4 Document the small-project code conventions.** [Work item](work-items/phase-0/0.4-code-conventions.md)

**Gate:** the existing placeholder app builds, and the test runner is easy to invoke by a documented command.

## Phase 1 — Application and database foundation

Goal: start and stop cleanly, open a local SQLite database, and evolve its schema safely.

- [x] **1.1 Add an application error type.** [Work item](work-items/phase-1/1.1-application-error-type.md)
- [x] **1.2 Add a SQLite connection lifecycle wrapper.** [Work item](work-items/phase-1/1.2-database-connection.md)
- [x] **1.3 Define and implement database-path resolution.** [Work item](work-items/phase-1/1.3-database-path.md)
- [x] **1.4 Enable and verify SQLite foreign keys.** [Work item](work-items/phase-1/1.4-foreign-keys.md)
- [x] **1.5 Add schema versioning and a migration runner.** [Work item](work-items/phase-1/1.5-migration-runner.md)
- [x] **1.6 Add the empty initial migration.** [Work item](work-items/phase-1/1.6-initial-migration.md)
- [x] **1.7 Prove migration failures roll back.** [Work item](work-items/phase-1/1.7-migration-rollback.md)
- [x] **1.8 Own the Notcurses initialize/stop lifecycle.** [Work item](work-items/phase-1/1.8-notcurses-lifecycle.md)
- [x] **1.9 Replace the placeholder with an empty TUI shell.** [Work item](work-items/phase-1/1.9-empty-tui-shell.md)
- [x] **1.10 Separate application lifecycle from interactive terminal input.** [Work item](work-items/phase-1/1.10-application-test-seam.md)

**Gate:** a new local database can be opened and migrated; the app handles startup/shutdown failures without crashing or creating fake records.

## Phase 1 follow-up — Foundation hardening

Phase 0 and Phase 1 are implemented, but the implementation review found reproducibility, build hygiene, and a few API gaps worth addressing before the feature code grows. These are new follow-ups, not changes to the completed Phase 0/1 records. Each has its own work file under [`work-items/foundation-hardening/`](work-items/foundation-hardening/).

### Recommended before or at Phase 2 start

- [x] **FH-01 Reproducible TUI smoke harness.** Replace the temporary pty responder with a checked-in script using tmux or a fuller terminal emulator. [Work item](work-items/foundation-hardening/FH-01-reproducible-tui-smoke.md)
- [x] **FH-03 Clean check script.** Automate fresh configure → build → CTest. [Work item](work-items/foundation-hardening/FH-03-clean-check-script.md)
- [x] **FH-04 CI.** Run formatting, static analysis, build/tests, and the TUI smoke test in GitHub Actions. [Work item](work-items/foundation-hardening/FH-04-continuous-integration.md)
- [x] **FH-05 Test-support target.** Remove integration-test headers from `apl_db`'s public include paths. [Work item](work-items/foundation-hardening/FH-05-test-support-library.md)
- [x] **FH-06 Temporary test directories.** Honor `TMPDIR` and clean nested test artifacts safely. [Work item](work-items/foundation-hardening/FH-06-test-temp-directories.md)
- [x] **2.0.1 TUI plane/resize decision.** Decide the screen layout and resize/re-render model before building Phase 2 screens. [Work item](work-items/phase-2/2.0.1-tui-layout-resize-decision.md)
- [x] **2.0.2 Mouse/focus decision.** Define mouse actions and their keyboard equivalents before implementing widgets. [Work item](work-items/phase-2/2.0.2-mouse-focus-decision.md)
- [x] **2.0.3 Repository ownership decision.** Define statement, connection, and transaction boundaries before team repository work. [Work item](work-items/phase-2/2.0.3-repository-ownership-decision.md)

### Useful hardening; can proceed alongside early Phase 2

- [x] **FH-02 Optional CTest TUI smoke.** Register the terminal smoke harness only behind an opt-in option. [Work item](work-items/foundation-hardening/FH-02-ctest-tui-smoke.md)
- [x] **FH-07 Sanitizer run.** Exercise the existing ASan/UBSan build options. [Work item](work-items/foundation-hardening/FH-07-sanitizer-test-run.md)
- [x] **FH-08 Formatting check.** Add `.clang-format` and a reproducible check. [Work item](work-items/foundation-hardening/FH-08-formatting-check.md)
- [x] **FH-09 Static-analysis baseline.** Run cppcheck or clang-tidy and triage findings. [Work item](work-items/foundation-hardening/FH-09-static-analysis-baseline.md)
- [x] **FH-10 Shared error-prefix helper.** Move migration error-context chaining into `src/core`. [Work item](work-items/foundation-hardening/FH-10-error-prefix-helper.md)
- [x] **FH-11 Distinct process exit codes.** Make failure categories machine-readable. [Work item](work-items/foundation-hardening/FH-11-process-exit-codes.md)
- [x] **FH-12 `--database` option.** Expose the existing configured database path from `main`. [Work item](work-items/foundation-hardening/FH-12-database-cli-option.md)

### Review findings already covered

- Error messages use a fixed 256-byte buffer, but truncation and NUL termination are explicitly documented and tested in 1.1; no size change is planned unless real usage shows the limit is inadequate.
- Path resolution tests the deterministic `_ex` function with injected XDG/HOME values. The environment-reading wrapper is documented as a thin `getenv` adapter; tests do not mutate process-global environment variables.
- Test binary policy is already documented in `CONTRIBUTING.md`: unit and SQLite integration tests share `apl1_tests`; terminal-independent TUI lifecycle tests use `apl1_tui_tests`.

## Phase 2 — Shared TUI shell and team workspaces

Goal: establish the navigation pattern and implement team creation and switching end to end.

Complete the 2.0.1 and 2.0.2 interaction decisions before implementing 2.1–2.4. Complete 2.0.3 before the team schema/repository work (2.5/2.6).

- [x] **2.0.1 Decide the TUI plane and resize model.** [Work item](work-items/phase-2/2.0.1-tui-layout-resize-decision.md)
- [x] **2.0.2 Decide the mouse and focus interaction model.** [Work item](work-items/phase-2/2.0.2-mouse-focus-decision.md)
- [x] **2.0.3 Define repository, statement, and transaction ownership.** [Work item](work-items/phase-2/2.0.3-repository-ownership-decision.md)

- [x] **2.1 Add the main navigation model.** [Work item](work-items/phase-2/2.1-navigation-model.md)
- [x] **2.2 Draw the shared screen frame.** [Work item](work-items/phase-2/2.2-shared-screen-frame.md)
- [x] **2.3 Add one reusable list/table interaction.** [Work item](work-items/phase-2/2.3-list-table-interaction.md)
- [x] **2.4 Add reusable confirmation and error dialogs.** [Work item](work-items/phase-2/2.4-confirmation-error-dialogs.md)
- [x] **2.5 Add the team table migration.** [Work item](work-items/phase-2/2.5-team-schema.md)
- [x] **2.6 Add team repository create/list/get operations.** [Work item](work-items/phase-2/2.6-team-repository-read-create.md)
- [x] **2.7 Add team use-case validation.** [Work item](work-items/phase-2/2.7-team-create-validation.md)
- [x] **2.9 Add persisted active-team setting.** Implement this before 2.8 so archiving the current team has a defined fallback. [Work item](work-items/phase-2/2.9-active-team-setting.md)
- [x] **2.8 Add team rename and archive operations.** Preserve IDs and history; depends on the active-team behavior in 2.9. [Work item](work-items/phase-2/2.8-team-rename-archive.md)
- [x] **2.10 Build the team list/switcher screen.** [Work item](work-items/phase-2/2.10-team-switcher-screen.md)
- [x] **2.11 Build the create-team form.** [Work item](work-items/phase-2/2.11-create-team-form.md)
- [x] **2.12 Build the team detail/edit screen.** [Work item](work-items/phase-2/2.12-team-detail-edit.md)
- [x] **2.13 Build the empty-database welcome flow.** [Work item](work-items/phase-2/2.13-empty-database-welcome.md)
- [x] **2.14 Add a team-isolation integration test.** [Work item](work-items/phase-2/2.14-team-isolation-test.md)
- [x] **2.15 Add a database code-coverage baseline.** After 2.5/2.6 land, use gcov to identify untested database paths without setting an arbitrary threshold. [Work item](work-items/phase-2/2.15-db-coverage-baseline.md)

**Gate:** the user can create two teams, switch between them, quit, relaunch, and see the selected team and separate data still present.

### Phase 2 verification (2026-11-24)

This record verifies the first implemented Phase 2 shell and team workflows. The
shell's header/body/actions/status layout was a completed baseline, but it is not
the final visual target. The selected window-bar/vertical-tabs/status-bar design
and command-line interactions are tracked separately below.

- `ctest --test-dir cmake-build-debug --output-on-failure`: 2/2 CTest tests pass
  (`apl1_unit_tests` 116/116 Criterion tests, `apl1_tui_tests` 25/25).
- `python3 scripts/smoke_tui.py --app cmake-build-debug/apl1`: passed the full
  gate flow — welcome, create Ferrari and McLaren, switch active team to Ferrari,
  quit with status 0, relaunch the same database, and verify Ferrari is still
  active. It also checks schema version 3 and the two persisted team rows.
- `./scripts/check.sh`, `./scripts/check-format.sh`, and
  `./scripts/check-static.sh` pass.
- `scripts/coverage.sh` records a 74.91% `src/db` line-coverage baseline in
  [2.15](work-items/phase-2/2.15-db-coverage-baseline.md); no threshold is enforced.

## Phase 2 follow-up — Modern TUI shell and ex-style commands

Goal: replace the first implemented screen arrangement with the product owner's
selected modern-TUI layout, while preserving the working team/data behavior and
the completed Phase 2 verification record above.

The target is a top **window/menu bar** with dropdowns, a left **vertical tab rail**,
a **main body** for the active view, and a bottom **status bar**. The active-team
switcher lives at the bottom of the tab rail and the team identity is repeated in
the status bar. Forms/selections use interactive dialogs. An ex-style `:` command
line supports both navigation shortcuts and structured actions: incomplete
commands open prefilled dialogs; complete valid commands execute the shared action.

- [x] **2.16 Redesign layout geometry.** [Work item](work-items/phase-2/2.16-redesign-layout-geometry.md)
- [x] **2.17 Add the window-bar dropdown menu model.** [Work item](work-items/phase-2/2.17-window-menu-model.md)
- [x] **2.18 Render and operate the window-bar dropdowns.** [Work item](work-items/phase-2/2.18-window-menu-rendering.md)
- [x] **2.19 Add the vertical view-tab rail.** [Work item](work-items/phase-2/2.19-vertical-view-tabs.md)
- [x] **2.20 Anchor the team switcher at the bottom of the tab rail.** [Work item](work-items/phase-2/2.20-team-switcher-rail.md)
- [x] **2.21 Implement the persistent status bar.** [Work item](work-items/phase-2/2.21-status-bar.md)
- [x] **2.22 Add a consistent color theme.** [Work item](work-items/phase-2/2.22-color-theme.md)
- [x] **2.23 Extend dialogs to support interactive forms.** [Work item](work-items/phase-2/2.23-interactive-form-dialogs.md)
- [x] **2.24 Move team create/edit/switch flows into dialogs.** [Work item](work-items/phase-2/2.24-team-flows-as-dialogs.md)
- [x] **2.25 Add the ex-style command line surface.** [Work item](work-items/phase-2/2.25-ex-command-line-ui.md)
- [x] **2.26 Add the command parser and registry.** [Work item](work-items/phase-2/2.26-command-parser-registry.md)
- [x] **2.27 Implement navigation and team command actions.** [Work item](work-items/phase-2/2.27-core-command-actions.md)
- [x] **2.28 Verify the redesigned shell and Phase 2 interaction parity.** [Work item](work-items/phase-2/2.28-redesign-smoke-and-parity.md)

**Redesign gate:** all primary views are reachable from vertical tabs; window-bar
menus open and operate; active team is switchable from the rail/status context;
dialogs work with keyboard and mouse; colors have a non-color fallback; and complete
versus incomplete ex commands behave as documented without bypassing business logic.

## Phase 3 — Shared records, people, and audit groundwork

Goal: add only reference records needed by money, development, parts, and outcomes, and establish traceable changes.

- [ ] **3.1 Add a season/period record.** User-created label and optional dates; test that the application does not generate seasons.
- [ ] **3.2 Add a driver profile repository and use case.** Create, view, rename, and archive a driver with minimal optional fields.
- [ ] **3.3 Add dated team-driver membership.** Record joins/leaves so historical results can keep their original team/driver association.
- [ ] **3.4 Add a car/chassis reference record.** User-created identifier and optional notes; no auto-created car.
- [ ] **3.5 Add an audit/activity table and repository.** Store timestamp, team, action, entity type/ID, and a concise description; test persistence and team scoping.
- [ ] **3.6 Record team create/rename/archive activity.** Verify the audit entry is written atomically with the team operation.
- [ ] **3.7 Define which fields get before/after audit values.** Start with team and financial changes; do not build a generic diff engine before a real use case needs it.
- [ ] **3.8 Add the people/reference list and detail views.** Reuse list/form patterns and clearly distinguish archived from active references.

**Gate:** teams, drivers, seasons, and cars are user-created references; historical membership is retained; important team changes appear in history.

## Phase 4 — Money ledger and budgets

Goal: deliver a trustworthy, testable money-management loop before adding dependent cost links.

- [ ] **4.1 Choose the v1 amount/currency representation.** Document precision, display rules, and the one-currency-per-team or explicit-conversion policy before schema work.
- [ ] **4.2 Add transaction/category schema.** Support user-entered income, expense, and opening-balance entries with dates, category, description, and team scope.
- [ ] **4.3 Add transaction repository create/list/get.** Use parameterized SQL and test values, ordering, filters, and team isolation.
- [ ] **4.4 Add amount and required-field validation.** Test zero, negative, precision, blank, and boundary cases according to the chosen money rules.
- [ ] **4.5 Add opening-balance use case.** Represent it as an explicit ledger entry; do not expose a separately editable balance field.
- [ ] **4.6 Add income and expense use cases.** Save the transaction and audit activity atomically; do not block recording an actual expense merely because it exceeds a budget.
- [ ] **4.7 Add balance and period/category totals.** Derive values from ledger entries and test empty ledgers, filters, and corrections.
- [ ] **4.8 Add transaction list and detail screens.** Show amount/date/category/description, current filters, and related history.
- [ ] **4.9 Add guided income, expense, and opening-balance forms.** Review team/currency and entered details before save.
- [ ] **4.10 Add a traceable transaction-correction workflow.** Prefer reversal/adjustment entries; test that prior balance history remains explainable.
- [ ] **4.11 Add budget-plan schema and repository.** Store user-entered period/category limits separately from actual transactions.
- [ ] **4.12 Add budget-plan validation and save use case.** Keep plan edits auditable and test category/period scope.
- [ ] **4.13 Add budget-versus-actual report calculations.** Show plan, actual, and variance with filters and an overspend warning; never count a plan as actual spending.
- [ ] **4.14 Add budget list/edit forms and report screen.** Make no-plan and no-actual states explicit.

**Gate:** a user can enter an opening balance, post income/expenses, correct a posted amount traceably, and compare actuals with a manually entered budget.

## Phase 5 — Development records

Goal: record work and relate it to money and cars/parts without fabricating progress or performance.

- [ ] **5.1 Add development-project schema and repository.** Support user-entered title, optional category/description/dates, status, notes, team, and optional season/car links.
- [ ] **5.2 Add project create/edit/archive use cases.** Validate references and preserve audit history.
- [ ] **5.3 Add dated development-update schema and repository.** Each update has a date, summary, optional status change, and notes.
- [ ] **5.4 Add project update use case.** Save update and audit history atomically; do not calculate a progress percentage.
- [ ] **5.5 Link actual money transactions to a project.** Ensure reports count a linked expense once, not once in each feature.
- [ ] **5.6 Add project list and filters.** Filter by status, season, and category where available.
- [ ] **5.7 Add project detail/timeline view.** Show entered project details, dated updates, and linked ledger transactions.
- [ ] **5.8 Add project create/update forms and command handoff.** Both entry points use the same use cases.

**Gate:** users can create a project, log successive updates, link recorded costs, and review the timeline without simulated progress.

## Phase 6 — Parts and replacement history

Goal: track component identity and lifecycle changes without inferring inventory or reliability.

- [ ] **6.1 Add part-type schema and repository.** Category/name are user-entered; serial number is not mandatory.
- [ ] **6.2 Add individual part/unit schema.** Link to a type and team; allow optional serial/part number and notes.
- [ ] **6.3 Add fitment/lifecycle-event schema.** Record install/remove/replace/repair/dispose events with user-entered date and optional car, position, event, reason, usage, and notes.
- [ ] **6.4 Add lifecycle repository queries.** Read current fitment and chronological history without overwriting old entries.
- [ ] **6.5 Add install/remove use cases.** Validate references and save history atomically; test invalid transitions and rollback.
- [ ] **6.6 Add replacement use case.** Link old-unit removal and new-unit installation in one transaction; verify a failure leaves the old fitment unchanged.
- [ ] **6.7 Link a replacement cost to one ledger transaction.** Display the relationship without double-counting costs.
- [ ] **6.8 Add current-fitment table and part-unit detail view.** Distinguish unknown/unrecorded fitment from “no part fitted.”
- [ ] **6.9 Add replacement workflow form.** Review car/position, old/new unit, date, and optional links before atomic save.
- [ ] **6.10 Add parts-history filters.** Search part number/type and filter by car, date, and action.

**Gate:** a replacement changes the recorded current fitment while preserving inspectable old/new unit histories and linked costs.

## Phase 7 — Events and manually entered outcomes

Goal: let the user organize events and record, correct, and review driver outcomes; no simulation.

- [ ] **7.1 Add user-created event schema and repository.** Store entered name/date plus optional season, location, and notes.
- [ ] **7.2 Add user-created session schema and repository.** Support a session label without generating a race weekend.
- [ ] **7.3 Add event/session create and edit use cases.** Validate team/season references and preserve history.
- [ ] **7.4 Decide and document the initial result-field set.** Keep optional fields optional; do not implement configurable custom fields until a concrete need is established.
- [ ] **7.5 Add driver-outcome schema and repository.** Store only entered values and stable links to driver, event, and session.
- [ ] **7.6 Add outcome validation and save use case.** Handle duplicate driver/session entries explicitly and preserve missing values as missing.
- [ ] **7.7 Add batch outcome-entry operation.** Validate all rows and save all-or-none; test rollback on one invalid row.
- [ ] **7.8 Add outcome correction history.** Show old/new values and audit corrections.
- [ ] **7.9 Add event/session detail and outcome table.** Clearly show which drivers have records and which have no recorded outcome.
- [ ] **7.10 Add event and quick outcome-entry forms.** Support single-driver editing and multi-driver entry without requiring every result field.
- [ ] **7.11 Add recorded-value summaries.** Aggregate only fields with defined semantics; label filters and omit/identify missing data instead of treating it as zero.

**Gate:** users can enter an event and its outcomes manually, correct an outcome, and inspect honest summaries without generated schedules/results or an implicit points system.

## Phase 8 — Search, history, and feature command routes

Goal: make stored information quick to retrieve and extend the Phase 2 command registry with domain-specific actions.

- [ ] **8.1 Add a global history/activity list.** Filter by team, record type, and date; open the affected record.
- [ ] **8.2 Add before/after details for audited edits.** Begin with team and money records and expand only when feature-specific audit needs are clear.
- [ ] **8.3 Add section-level text search for teams and people.** Preserve filters on return from detail.
- [ ] **8.4 Add money filters and search.** Support date/period, category, type, and description.
- [ ] **8.5 Add development/parts/event filters.** Add per-section fields incrementally rather than building a universal query language.
- [ ] **8.6 Add money and budget command routes.** Complete commands execute through their use cases; incomplete commands open prefilled dialogs.
- [ ] **8.7 Add development and parts command routes.** Use the same forms and use cases as the menu path.
- [ ] **8.8 Add event, outcome, and history command routes.** Preserve team scope and missing-data semantics.
- [ ] **8.9 Extend command help/completion from the registry.** Cover each newly implemented domain command and usage error.
- [ ] **8.10 Test money/budget menu-command parity.** Include a complete command and an incomplete command that opens a dialog.
- [ ] **8.11 Test development/parts menu-command parity.** Include validation and linked-record behavior.
- [ ] **8.12 Test event/outcome/history menu-command parity.** Include ambiguity and missing-field behavior.

**Gate:** the user can discover commands, route a common operation to a guided form, and see identical validation/persistence whether they started from a menu or command.

## Phase 9 — MVP review and stability

Goal: verify the integrated product against the requirements before expanding scope.

- [ ] **9.1 Add first-run and empty-state review.** Walk through a new database and ensure every empty view offers an appropriate next action without creating data.
- [ ] **9.2 Add end-to-end team isolation workflow test.** Verify team A's money, parts, and events never appear in team B's default views.
- [ ] **9.3 Add end-to-end money workflow test.** Opening balance → expense → correction → budget report, including persistence after reopen.
- [ ] **9.4 Add end-to-end development/parts workflow test.** Log a project update and part replacement with linked transaction and inspect their history.
- [ ] **9.5 Add end-to-end event/outcome workflow test.** Create event/session/outcomes, correct one, and inspect missing-data behavior.
- [ ] **9.6 Review database error handling.** Exercise locked/unwritable/corrupt or migration-failure cases where practical; show actionable messages and avoid partial writes.
- [ ] **9.7 Review terminal usability.** Test keyboard-only, mouse use, small terminal sizes, resize, and no-color operation.
- [ ] **9.8 Document database location and backup/restore guidance.** Do not claim backup support until it is tested.
- [ ] **9.9 Recheck every MVP acceptance criterion in `REQUIREMENTS.md`.** Record gaps as new small tasks rather than silently widening the release.

**Gate:** the MVP acceptance criteria pass, user data survives restart, and failures do not silently corrupt or partially apply a record.

## Later, deliberately not decomposed yet

Only split these into implementation-sized tasks after the MVP reveals a user need:

- CSV export/import, saved views, and richer reporting.
- User-configurable categories, custom result fields, scoring rules, or multiple currencies.
- More detailed contracts/personnel, stock/inventory movements, and attachments.
- Database backup automation, portability, and migration support for releases.
- Advanced filtering, cross-team reports, and additional Notcurses refinements.

These are not implicit commitments for v1. When one becomes relevant, define its user workflow and acceptance criteria first, then split it into small slices with tests.


### Phase 2 follow-up verification (2026-11-24)

- `./scripts/check.sh`, `./scripts/check-format.sh`, and
  `./scripts/check-static.sh` pass.
- `ctest --test-dir cmake-build-debug --output-on-failure`: 2/2 tests pass
  (`apl1_tests` 116/116, `apl1_tui_tests` 77/77).
- `python3 scripts/smoke_tui.py --app cmake-build-debug/apl1`: passes the
  redesigned menu/tab/status/dialog/command flow, resize-with-open-modal
  checks, persistence, and a limited-color (`xterm-mono`) launch.
- All 2.16-2.28 follow-up items are complete; the redesign gate is met.
