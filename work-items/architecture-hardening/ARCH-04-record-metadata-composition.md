# ARCH-04 — Shared record metadata

**Status:** Implemented  
**Target:** Before work item 5.1

## Goal

Stop repeating archive state and creation/update timestamps in every persisted
record type. New Phase 4–7 records should compose one metadata block instead of
copying four fields.

## Work list

- [x] Add `apl_record_meta` to `src/domain/record.h`.
- [x] Compose it into team, season, driver, car, and membership values.
- [x] Keep `APL_TEAM_TIMESTAMP_MAX` and `APL_REFERENCE_TIMESTAMP_MAX` as aliases
      for existing callers.
- [x] Update repositories, services, TUI details, and tests to use `meta.*`.

## Acceptance checks

- [x] Existing repository and audit tests pass without schema changes.
- [x] Timestamp buffers still use the shared 32-byte size.
- [x] Archive behavior for team/season/driver/car is unchanged.
- [x] Formatting and static-analysis checks pass.

## Boundaries

- Membership currently has no archive action; its `meta.archived` /
  `meta.archived_at` fields remain unused until the domain needs them.
- This item does not change table columns or SQL.
