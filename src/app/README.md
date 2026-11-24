# src/app/

Application and use-case coordination. Composes database setup, migration, TUI
lifecycle, and domain operations into the application's startup/shutdown flow.

Business rules live in `src/domain`; SQL lives in `src/db`; drawing/input lives in
`src/tui`. `src/app` coordinates them without reimplementing their logic.

Use-case functions that perform multiple writes own the transaction boundary
(`apl_db_transaction_begin/commit/rollback`); repositories are single-operation
primitives. Team create/update/archive/active-selection use cases live in
`team_service.{h,c}`. Phase 3 use cases live in `season_service`, `driver_service`,
`membership_service`, `car_service`, and the read-only `activity_service`. See
[2.0.3](../../work-items/phase-2/2.0.3-repository-ownership-decision.md).
