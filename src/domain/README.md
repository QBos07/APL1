# src/domain/

Business rules and domain types: validation and use cases that menus and commands
must both go through. Independent of SQLite and Notcurses.

Team identity normalization/validation lives in `team.{h,c}`. Shared reference
rules for seasons/periods, drivers, memberships, cars, and audit values live in
`reference.{h,c}` (trimming, length limits, and real ISO date/range validation).
Tests for this code live in `tests/unit` and run without a terminal or database.
