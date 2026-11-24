# src/app/

Application and use-case coordination. Composes database setup, migration, TUI
lifecycle, and domain operations into the application's startup/shutdown flow.

Business rules live in `src/domain`; SQL lives in `src/db`; drawing/input lives in
`src/tui`. `src/app` coordinates them without reimplementing their logic.

Use-case functions that perform multiple writes own the transaction boundary
(`apl_db_transaction_begin/commit/rollback`); repositories are single-operation
primitives. See
[2.0.3](../../work-items/phase-2/2.0.3-repository-ownership-decision.md).
