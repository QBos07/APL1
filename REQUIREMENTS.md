# Product requirements: Formula 1 team management database

## 1. Product direction

Build a local, user-maintained database for managing one or more Formula 1 teams. It is a practical record-keeping and management tool, not a race or season simulator. The user enters all team, season, event, financial, development, replacement, and driving-outcome information by hand. The application stores it, validates it, and presents useful summaries, tables, and history; it does not invent missing data or fetch official data from external services.

The frontend is a Notcurses terminal UI backed by SQLite. It should support both conventional menus/forms/tables and a command prompt where users can type concise commands to perform the same actions.

## 2. Product principles

- **User-entered source of truth:** Do not generate teams, rosters, events, results, budgets, or other domain facts. Ask the user to enter required information. Derived totals and summaries are fine when clearly identified as calculated.
- **Fast to record, easy to review:** Common actions should be available through menus/forms and concise commands. Recorded information should be easy to search, filter, and inspect later.
- **Clear team context:** The active team is always visible and can be switched quickly, like switching workspaces/accounts in a messaging app.
- **History matters:** Financial and operational records should retain their history. Corrections should be traceable rather than silently erasing important facts.
- **One business-logic path:** Menus and commands call the same validated business operations; neither interface bypasses domain rules.
- **Local-first and safe:** Data persists in SQLite, related changes are transactional, and destructive actions require confirmation.

## 3. Intended user and scope

V1 is for one person working locally. Multiple teams are separate workspaces in one database; there are no online accounts, network services, live data feeds, or collaborative editing requirements. The app can be used for real or fictional teams, with the user responsible for entering the records.

No official F1 integration or licensing is assumed. The application is a flexible management database and makes no claim of official affiliation.

## 4. Core features

### 4.1 Teams and active team

- Create and manage multiple team records. Required fields are collected from the user; do not silently seed sample teams or pretend defaults are real data.
- Switch the active team from a persistent team picker or a short command. Persist the selection across application restarts.
- Show the active team name prominently on every main screen and scope team-specific views and commands to it.
- Provide a team list with useful user-entered or calculated summaries, such as current balance and latest recorded activity.
- Archive a team without destroying its records. Confirm archive and any permanent deletion. Do not allow accidental removal of a team's history.
- The team switcher should feel like switching workspaces: show team names plus a few useful facts, mark the current team, make switching a single selection, and return to the previous screen in the newly selected team. Switching must not copy, reset, or merge data.
- A team profile should have a detail page with editable identity/contact notes, user-created seasons, currency choice, archived status, and links to its people, cars, and records. Keep optional information genuinely optional.

### 4.2 Team information and reference records

- Maintain editable team profile information such as name, season/period labels, personnel, drivers, and notes. Let the user decide which seasons and events to create; do not generate a calendar.
- Allow the user to add, edit, search, and archive reference records (for example, drivers, staff, events, circuits, car/chassis identifiers, and components) as applicable.
- Avoid requiring fields the user does not need. Mark required fields clearly, support optional notes, and validate values and references before saving.
- Keep records linked to stable IDs rather than names so renaming a person, event, or part does not break history.
- Represent driver/team involvement with dated membership or contract records rather than only a current name on a team. This lets old outcomes remain associated with the right team and driver after a roster change.
- Let the user create season/period labels and decide which records belong to them. A season is an organizing label, not a generated schedule or an assumed calendar year.

### 4.3 Money management

- Record income and expenses manually with amount, currency, date, category, description, and optional links to a team, event, development item, or part replacement.
- Show a calculated balance and transaction totals by configurable period/category. The user enters the opening balance explicitly; do not assume or seed one.
- Provide an explicit opening-balance workflow that creates a clearly labeled ledger entry. From then on, the displayed balance is derived from the ledger, not independently editable.
- Allow the user to maintain budget plans by season/period and category. Show planned budget, recorded actual spending/income, and variance separately. Budget limits are informational: warn about overspending but do not prevent a user from recording a real expense.
- Keep budget plans separate from actual ledger entries so the same planned cost is not accidentally counted as money spent. If forecast/committed costs are added, label them distinctly and let the user mark or link them to a later actual transaction.
- Provide searchable, filterable transaction history and summaries for spending, income, and net change.
- Do not edit a balance as an untraceable number. Corrections to posted transactions should be represented by an adjustment/reversal or an auditable edit that preserves the prior value.
- Validate amount and currency input, explain invalid entries, and save a transaction and its history atomically.
- A transaction detail view should show all entered fields, related records, creation/correction history, and the effect it has on filtered totals. The user can duplicate a prior entry as a starting point, but must review and explicitly save it as a new transaction.
- Do not silently add unlike currencies. Either use one reporting currency per team in v1 or require the user to enter an explicit converted/reporting amount and exchange-rate note before including a foreign-currency transaction in totals.

### 4.4 Development and work records

- Let the user record development activities/projects, including a title/category, description, dates, status, associated car/component, costs, and free-form notes as appropriate.
- Allow progress updates over time and show a chronological project history, including status changes and related costs.
- Do not simulate research progress, performance gains, completion dates, or effects. Any reported outcome or measurement is entered by the user.
- Make development records linkable to financial transactions, events, and component records where relevant, without requiring every link.
- A project detail page should show its current user-entered status, description, target/actual dates if provided, budget if provided, linked actual costs, associated parts/car, notes, and a chronological update list.
- Let the user record individual work updates (date, summary, author/note if useful, optional cost and attachment reference) rather than overwriting the project description for each update. The current project view is a summary of those updates, not an invented progress percentage.
- Support user-defined project categories/status values or a small editable vocabulary; avoid hard-coding a development process the user may not follow.

### 4.5 Parts and replacements

- Maintain a component/part register for each team or car, with user-entered identity, category, serial/part number, condition/status, and notes.
- Record installation, removal, replacement, repair, or disposal as dated history entries, with the relevant part(s), reason, event/mileage/usage information if supplied, and optional cost.
- Preserve the component lifecycle: replacing a part must not overwrite or erase the previous part's history. Allow users to inspect current fitment and historical changes.
- Do not infer reliability, lifespan, stock levels, compatibility, or replacement recommendations unless the user records the relevant data and explicitly asks for a derived report.
- Distinguish a **part type** (for example, front wing assembly) from an **individual part/unit** (a specific serial-numbered item) when the user wants that detail. Do not force serial numbers when the user's records do not have them.
- The parts view should make it easy to answer: what is currently fitted to each car; what was removed/replaced; when and why; where the removed part went; and what cost was recorded. These answers come only from user-entered fitment and lifecycle events.
- A replacement workflow should select the car/position, choose or add the replacement unit, record the date/event and reason, and optionally record the removed unit's disposition and linked cost. Preview the change before saving; save the linked removal/installation history atomically.
- Provide a separate inventory/on-hand view only if the user records stock movements. Do not infer that a removed or unassigned part is available inventory.

### 4.6 Events, driving outcomes, and history

- Let the user manually create events and enter outcomes for each driver (for example, finish/status, position, points, laps, penalties, or notes). Fields should be configurable/optional enough to accommodate different record-keeping needs.
- Do not generate event schedules, starting grids, race results, points, or performance values. Do not simulate a race.
- Provide event history and driver/team summaries calculated only from recorded entries. Clearly distinguish entered values from derived aggregates.
- Allow corrections while preserving a change history, including what changed and when. An explicit audit/history view should make prior values and important actions inspectable.
- Where standings or totals are displayed, document their calculation and let the user configure or enter any scoring rules needed. Never imply that absent records mean zero or that the dataset is complete.
- Let users organize outcomes by user-created event and session (for example, practice, qualifying, sprint, race, test, or a custom label). Event and session names, dates, location, and order are entered by the user.
- Provide a practical outcome-entry form with one row per driver and user-configurable result fields. Common fields may include start/finish position, status, laps, points, penalty, fastest-lap flag/time, and notes; do not require all fields or calculate values the user did not enter.
- Support both a quick-entry workflow for entering several drivers at one event and a single-driver detail workflow for corrections or notes. Clearly show which drivers already have an entry and which do not; missing entries are “not recorded,” not DNFs or zero points.
- Result comparisons and totals must disclose the selected season/event/session filters and how unrecorded fields are treated. Do not apply an implicit points system; points are entered by the user or calculated only when they explicitly configure a scoring rule and choose to apply it.

### 4.7 Overview tables and reports

- Provide an overview for the active team with recent activity, current recorded balance, upcoming events only if entered by the user, and links to core records.
- Include useful tables/reports for finances, development activity, current and historical parts, events/outcomes, and change history.
- Support sorting and filtering on relevant columns, search by common fields, and clear empty/no-match states. Views must respect active-team scope unless the user deliberately selects an all-teams view.
- Make computed values identifiable and show enough context (period, filters, included records) to understand a total.
- Provide saved named filters/views where practical (for example, a season's expenses or all replacement events for one car) so frequent review does not require rebuilding filters.
- Allow opening a row to view the full record and its related history; support a clear back path to the same table position and filters.
- Offer a printable/exportable CSV view of filtered records as a follow-on or MVP feature if time permits. Exported data must respect the displayed filters and include headers; do not make export a prerequisite for ordinary in-app use.

### 4.8 Consistent record lifecycle

- Every record-entry form should clearly distinguish required and optional fields, show validation beside the relevant field, preserve typed values after a validation error, and offer explicit **Save** and **Cancel** actions.
- For a successful save, show a concise confirmation and open the new record or return to the originating list with the new row visible. Do not silently discard the user into an unrelated screen.
- For references used by other records, prefer **archive** over delete. Archived people, parts, categories, and events remain available in historical details but are excluded from default “current” pickers; provide a way to include them when needed.
- Maintain a chronological activity/history view with timestamp, action, record type/name, and a link to the affected record. For edits, retain changed fields and old/new values for important records. This is a local single-user audit history, not a security-grade compliance log.
- Empty, incomplete, and stale data should be clearly labeled. Do not show a precise “current” status if the user has not recorded the update needed to establish it.

## 5. TUI and command interface

### 5.1 Menu-driven interaction

- Main areas: **Overview**, **Teams**, **People**, **Events & Outcomes**, **Money**, **Development**, **Parts**, **History**, and **Settings**.
- Provide normal menus, forms, detail views, and tables with visible focus, validation feedback, and contextual actions.
- Mouse users can click controls, rows, tabs, and menus and scroll tables. Mouse is optional: core actions remain fully keyboard-operable.
- Support terminal resize, constrained layouts, readable focus/selection states, and a useful non-color mode.
- Use confirmations for destructive actions and provide a clear way to cancel forms. Avoid losing unsaved input without warning.
- Use a consistent screen pattern: section title and active-team context; a list/table or detail panel; available actions; and a message area for validation, save confirmation, or errors. Do not rely on color alone to communicate state.
- In list screens, keep the current filter/sort visible, show useful columns without requiring horizontal scrolling for basic tasks, and provide a detail view for the full record. Returning from a detail view should preserve the user's table position and filters.
- Provide an obvious **Add** action and contextual actions such as **View**, **Edit**, **Archive**, and **History**. Only expose actions valid for the selected record; explain unavailable actions rather than silently ignoring them.
- Show dates, amounts, statuses, and missing values consistently. Display missing values as “not recorded” or “—”, never as a misleading zero or default status.

### 5.2 Concise command entry

- Provide an always-discoverable command prompt/palette that accepts short, documented commands in addition to menus. It is a structured command interface, not an expectation that users must memorize key bindings or use natural-language parsing.
- Commands are contextual to the active team by default and can explicitly name/select another team. Show the target team before committing a write if scope could be ambiguous.
- Support command help, usage hints, command completion/suggestions where practical, and clear parse/validation errors. Do not execute arbitrary SQL or shell commands.
- Commands and menus must use the same business operations, validation, confirmation rules, and persistence paths.
- Illustrative command shapes (exact grammar can be finalized during implementation):
  - `team list` / `team switch <name>`
  - `money income` / `money expense` (open a guided transaction form) / `money list --category <category>`
  - `budget set` (open a guided budget form) / `budget report`
  - `development new` / `development log` (open a guided project or update form)
  - `part add` / `part replace` (open a guided lifecycle form)
  - `event add` / `result add` (open a guided event or outcomes form)
  - `history --type <record-type>` / `help result add`
- Incomplete commands should provide a brief usage hint and offer to open the corresponding guided form. A command such as `money list` may execute immediately; a command that writes data should either collect all values through a guided form or display a review/confirmation step before saving.
- Resolve names that match multiple records by asking the user to choose from a short list; do not guess. Support stable record IDs in command output so a user can disambiguate records when needed.
- Keep command history for the current session as a convenience, but do not store sensitive typed text or treat command history as the audit log.

### 5.3 Overview and record screens

- **Overview:** show the active team and selected season/period, current ledger balance, budget-versus-actual summary when a budget exists, recent activity, and user-entered upcoming events. Each summary links to its underlying records. Show a clear setup prompt instead of empty-looking widgets when the team has no data.
- **Team switcher:** show a selectable list of teams, active marker, and small facts such as season label, balance/currency if known, and most recent record date. Include create-team and archived-team actions. Switching returns to the previous section for the new team, with no write performed merely by highlighting a team.
- **List/table screens:** include search, useful filters, sort indicators, row count, and a clear empty state. Filters should be easy to clear. For tables with many columns, prioritize the most useful columns and expose the full record on selection rather than making core workflows depend on horizontal scrolling.
- **Record detail:** show the complete entered record, linked records, calculated values with a short explanation, and chronological history. Provide edit/archive/correct actions where allowed. Related-record links should let the user navigate to another record and return to the original context.
- **Forms:** group related fields, indicate which are required, allow optional notes without blocking save, and explain accepted formats. Keep input after errors. A form opened from a command should behave like the same form opened from a menu.
- **History:** provide a searchable activity timeline with filters for date range, record type, and action. Opening an entry should show the affected record and, for tracked edits, the before/after values.

## 6. Typical user workflows

### 6.1 Start using the database

1. On an empty database, show a short welcome explaining that no team or motorsport data is preloaded.
2. Offer **Create team** and **Quit**. Creating a team asks for a team name and any truly required configuration (for example, reporting currency); season, roster, budget, events, and starting balance are entered now or later by the user, never fabricated.
3. After saving, open the team overview with clear next actions such as **Add a driver**, **Create a season**, **Record opening balance**, or **Add an event**. These are prompts, not automatic record creation.
4. On later launches, restore the last active team. If no team is active/available, explain why and let the user choose or create one.

### 6.2 Record and review money

1. From Money, choose **Income**, **Expense**, or **Opening balance**; a command such as `money expense` opens the same form.
2. Enter amount, date, category, description, and any relevant season/event/project/part links. The user explicitly enters the amount and date; a form may show formatting guidance but must not silently invent business data.
3. Before saving, show a concise summary of the transaction and the team/currency it will affect. Saving updates the ledger and audit history together.
4. Return to the transaction list with the new row selected and the displayed balance refreshed. The user can filter by period/category, open the transaction, or add a correction/reversal.
5. In Budget, the user creates a period/category plan and enters the planned amount. A report compares that entered plan with ledger transactions in the same scope and shows variance. Overspending is a visible warning, not a blocked expense.

### 6.3 Track development work

1. Create a project with a user-entered name and optional category, description, dates, planned budget, car/part links, and notes.
2. Add dated progress updates as work occurs; each update can include a summary, status change, cost, and links. The project detail page shows the update timeline and related actual transactions.
3. If a cost is entered as a financial transaction, link it to the project so reports can include it once. Do not also add an independent “actual cost” to totals unless the UI clearly marks it as informational and prevents double-counting.
4. Closing/completing a project is a user action. The app records the entered completion date/status and does not claim a performance improvement.

### 6.4 Record a part change

1. Open Parts, select a car/fitment position (or create the reference), and choose **Replace**, **Install**, **Remove**, or **Repair**.
2. Select an existing individual unit or enter a new one; serial number and usage are optional unless the user configures them as required.
3. Enter the date and optional event, reason, usage/mileage, disposition, notes, and cost/transaction link. Review the proposed old/new fitment before committing.
4. Save a linked lifecycle entry so the current-fitment view changes and the previous unit remains visible in history. A failed save changes neither fitment nor history.
5. From either unit's detail page, inspect its full lifecycle and related costs/events.

### 6.5 Record event and driver outcomes

1. Create an event by entering its name/date and optional venue, season, and notes. Add a session when needed; the app does not create an expected calendar or race weekend.
2. Open **Enter outcomes** for a session. Select the participating drivers and fill only the fields the user has information for. Provide clear missing/not-recorded states.
3. Review all rows before saving. Saving a batch either records all valid rows or none; duplicates for the same driver/session should be clearly identified and the user asked whether to edit the existing result or create a distinct attempt/session.
4. The event page shows entered outcomes, related part changes/development/financial records, and optional aggregates. Every aggregate identifies its scope and does not convert missing fields to zero.
5. A correction opens the existing outcome, shows the prior value, and records the correction in history.

### 6.6 Find and correct historical information

1. Search globally within the active team or open a section-specific list. Search should match useful text such as names, descriptions, categories, event labels, and part numbers.
2. Narrow results by season/period, date range, status, category, driver, car, or linked record where applicable; display active filters and result count.
3. Open a result to inspect its details and related history. Return to the same filtered list position after viewing it.
4. Correct a record through its detail action. Show what will change and preserve history for important changes; for posted money use a correction/reversal workflow rather than silently changing historical balance.

## 7. Data and technical requirements

- SQLite is the source of truth for teams, user-created reference records, financial transactions, development records, component lifecycle entries, events/outcomes, settings, and audit history.
- Use schema migrations, stable primary keys, foreign-key enforcement, and appropriate indexes for team scoping, dates, categories, and common searches.
- Use transactions for related writes. Keep business rules in testable C business-logic/use-case code, not only in TUI handlers or database triggers.
- Menus and commands must share repositories/use cases so behavior cannot diverge by input method.
- Preserve history for important records. Prefer archival or reversal/correction records over destructive deletion when a record has dependent history. Document any permitted hard-delete behavior.
- Handle empty databases, migration failures, invalid user input, and SQLite errors gracefully. Never silently save a partial operation or fabricate replacement data.
- Keep the app offline in normal use. Document where the database is stored and how to back it up before a stable release.

## 8. Quality and testing requirements

- Build with the existing CMake setup, C23, and strict compiler warnings.
- Unit-test business logic with Criterion independently of Notcurses and SQLite where possible. Use repository/integration tests with temporary SQLite databases for persistence and transaction behavior.
- Test at least: active-team persistence and team isolation; money calculations and correction history; validation of required fields and references; part replacement history; development updates; manually entered outcomes and derived summaries; audit records; and atomic failure behavior.
- Test the command parser/dispatcher separately, including valid commands, incomplete commands that should open guidance, invalid input, team scoping, and parity with menu actions.
- On first launch, explain that the database is empty and guide the user to create their own team and records. Do not populate demo data unless explicitly offered as an opt-in sample database clearly separated from real records.

## 9. MVP acceptance criteria

1. A user can create multiple teams with manually entered details, switch the active team, restart the app, and return to the previously active team.
2. The active team is visible throughout the interface, and team-scoped views and writes do not leak across teams.
3. A user can manually record income and expenses, see a correctly calculated balance/history, and correct an entry without losing an audit trail.
4. A user can log development work and part replacements and later inspect their chronological history without prior records being overwritten.
5. A user can create an event and manually enter driver outcomes, then review the entered records and clearly labeled derived summaries. No event or result is generated by the application.
6. Core records can be entered and found through both the menu/form flow and concise commands, using the same validation and business rules.
7. Search, filters, overview tables, and history help the user review their team's records; empty or incomplete datasets are represented honestly.
8. Automated tests cover the business rules, command behavior, team isolation, and database-backed workflows described above.

## 10. Out of scope for v1

- Race/season simulation, generated schedules or results, AI suggestions, and automatic performance or reliability projections.
- Live timing, external APIs, web services, cloud sync, online accounts, and multiplayer.
- Arbitrary SQL or shell access through the command prompt.
- Automatically seeded sample teams or official championship datasets.
- Detailed telemetry analysis, predictive analytics, and automatic regulatory/compliance decisions.

## 11. Suggested delivery order

1. **Foundation:** migrations, empty-database first-run flow, team profiles, active-team switching, and shared business-operation layer.
2. **Record keeping:** people/reference records, money ledger, development logs, part register and replacement history.
3. **Events and review:** user-created events/outcomes, overview tables, filtering/search, and audit/history views.
4. **Interaction and hardening:** command palette/parser, menu-command parity, mouse support, resize/accessibility, backup guidance, and broader tests.

## 12. Decisions to settle during implementation

- Which money currencies, categories, and reporting periods should be built in versus user-configurable?
- Which event/outcome fields are required for the user's intended workflow, and should teams be able to define custom fields?
- Should corrections be made with append-only reversals, auditable edits, or both? The recommendation is append-only for money and auditable edits for descriptive records.
- What should command syntax look like after trying the common workflows? Keep commands short, discoverable, and capable of handing off to guided forms.
