# src/db/

SQLite persistence boundary: connection lifecycle, database-path resolution,
schema migrations, and repositories. This is the only area of the project that
executes SQL.

Rules:

- Translates SQLite failures into the application error type from `src/core`.
- Never calls Notcurses or prints to the terminal.
- Uses parameterized SQL and transactions for related writes.
- The application owns the connection; repositories borrow it and never open or
  close it. Each repository call owns and finalizes its statement locally.
  Repositories are single-operation primitives; use cases own multi-write
  transactions through the small `db/transaction` helper. See
  [2.0.3](../../work-items/phase-2/2.0.3-repository-ownership-decision.md).
- Team and active-team-setting persistence lives in `team_repository.{h,c}` and
  `settings_repository.{h,c}`. Phase 3 reference persistence lives in
  `season_repository`, `driver_repository`, `membership_repository`, and
  `car_repository`; append-only activity/field-change persistence lives in
  `activity_repository`.
