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

- [ ] **0.1 Verify the current configure/build/test commands.** [Work item](work-items/phase-0/0.1-baseline-build-test.md)
- [ ] **0.2 Rename the test target and CTest test.** [Work item](work-items/phase-0/0.2-test-target-names.md)
- [ ] **0.3 Confirm the initial source layout.** [Work item](work-items/phase-0/0.3-source-layout.md)
- [ ] **0.4 Document the small-project code conventions.** [Work item](work-items/phase-0/0.4-code-conventions.md)

**Gate:** the existing placeholder app builds, and the test runner is easy to invoke by a documented command.

## Phase 1 — Application and database foundation

Goal: start and stop cleanly, open a local SQLite database, and evolve its schema safely.

- [ ] **1.1 Add an application error type.** [Work item](work-items/phase-1/1.1-application-error-type.md)
- [ ] **1.2 Add a SQLite connection lifecycle wrapper.** [Work item](work-items/phase-1/1.2-database-connection.md)
- [ ] **1.3 Define and implement database-path resolution.** [Work item](work-items/phase-1/1.3-database-path.md)
- [ ] **1.4 Enable and verify SQLite foreign keys.** [Work item](work-items/phase-1/1.4-foreign-keys.md)
- [ ] **1.5 Add schema versioning and a migration runner.** [Work item](work-items/phase-1/1.5-migration-runner.md)
- [ ] **1.6 Add the empty initial migration.** [Work item](work-items/phase-1/1.6-initial-migration.md)
- [ ] **1.7 Prove migration failures roll back.** [Work item](work-items/phase-1/1.7-migration-rollback.md)
- [ ] **1.8 Own the Notcurses initialize/stop lifecycle.** [Work item](work-items/phase-1/1.8-notcurses-lifecycle.md)
- [ ] **1.9 Replace the placeholder with an empty TUI shell.** [Work item](work-items/phase-1/1.9-empty-tui-shell.md)
- [ ] **1.10 Separate application lifecycle from interactive terminal input.** [Work item](work-items/phase-1/1.10-application-test-seam.md)

**Gate:** a new local database can be opened and migrated; the app handles startup/shutdown failures without crashing or creating fake records.

## Phase 2 — Shared TUI shell and team workspaces

Goal: establish the navigation pattern and implement team creation and switching end to end.

- [ ] **2.1 Add the main navigation model.** Define sections and current-screen state independently of drawing code.
- [ ] **2.2 Draw the shared screen frame.** Include section title, active-team context, message/status area, and available actions.
- [ ] **2.3 Add one reusable list/table interaction.** Support focus, selection, scrolling, empty state, and resize without implementing every feature table.
- [ ] **2.4 Add reusable confirmation and error dialogs.** Test the interaction/state behavior separately from domain rules.
- [ ] **2.5 Add the team table migration.** Include stable ID, user-entered name, optional profile notes, archive state, and timestamps; no default team row.
- [ ] **2.6 Add team repository create/list/get operations.** Test them against temporary SQLite databases.
- [ ] **2.7 Add team use-case validation.** Reject blank names and disallowed duplicates; keep these rules outside the TUI and test boundaries.
- [ ] **2.8 Add team rename and archive operations.** Preserve IDs and related history; test archived teams remain retrievable for history.
- [ ] **2.9 Add persisted active-team setting.** Test selection survives closing and reopening the database; handle missing/archived selection explicitly.
- [ ] **2.10 Build the team list/switcher screen.** Show the active marker, current team name, create action, and archived-team access.
- [ ] **2.11 Build the create-team form.** Save only fields the user entered and show validation errors without clearing typed values.
- [ ] **2.12 Build the team detail/edit screen.** Show profile details and links/placeholders for team sections without inventing their data.
- [ ] **2.13 Build the empty-database welcome flow.** Offer create-team or quit, and guide the user to optional next actions after creation.
- [ ] **2.14 Add a team-isolation integration test.** Create two teams and prove team-scoped reads/writes do not cross boundaries.

**Gate:** the user can create two teams, switch between them, quit, relaunch, and see the selected team and separate data still present.

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

## Phase 8 — Search, history, and concise commands

Goal: make stored information quick to retrieve and let command users invoke the same workflows as menu users.

- [ ] **8.1 Add a global history/activity list.** Filter by team, record type, and date; open the affected record.
- [ ] **8.2 Add before/after details for audited edits.** Begin with team and money records and expand only when feature-specific audit needs are clear.
- [ ] **8.3 Add section-level text search for teams and people.** Preserve filters on return from detail.
- [ ] **8.4 Add money filters and search.** Support date/period, category, type, and description.
- [ ] **8.5 Add development/parts/event filters.** Add per-section fields incrementally rather than building a universal query language.
- [ ] **8.6 Add command prompt UI.** Open/close it without losing the current screen context; display prompt, result/error, and help affordance.
- [ ] **8.7 Add command tokenizer/parser with unit tests.** Handle whitespace, quoted names, incomplete input, and clear syntax errors; never execute SQL or shell text.
- [ ] **8.8 Add command help and usage suggestions.** Test unknown and incomplete commands.
- [ ] **8.9 Add `team list` and `team switch` commands.** Use the same team use cases and active-team setting as the picker.
- [ ] **8.10 Add `money` list/add command routes.** Listing may run directly; writes open the same guided forms as the menu.
- [ ] **8.11 Add `budget` command routes.** Route plan/report actions through the same screens and use cases.
- [ ] **8.12 Add development and parts command routes.** Open their guided forms; do not add a second validation path.
- [ ] **8.13 Add event/outcome command routes.** Open the corresponding guided event/result forms.
- [ ] **8.14 Test menu/command parity for one workflow at a time.** Start with a money transaction, then repeat the pattern for each implemented domain.

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
