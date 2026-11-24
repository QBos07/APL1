# src/app/

Application and use-case coordination. Composes database setup, migration, TUI
lifecycle, and domain operations into the application's startup/shutdown flow.

Business rules live in `src/domain`; SQL lives in `src/db`; drawing/input lives in
`src/tui`. `src/app` coordinates them without reimplementing their logic.
