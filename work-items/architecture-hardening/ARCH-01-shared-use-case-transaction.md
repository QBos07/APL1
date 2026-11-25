# ARCH-01 — Shared write-use-case transaction plumbing

**Status:** Implemented  
**Target:** Before work item 4.6

## Goal

Keep atomic write behavior and error-context handling consistent across feature
use cases without copying `begin`/`commit`/`rollback` helpers into every
service.

## Work list

- [x] Add `src/app/use_case.[ch]` with shared context, begin, commit, and
      rollback helpers.
- [x] Replace the duplicated helpers in the car, driver, membership, season,
      and team services.
- [x] Preserve the existing contract: a failed commit rolls back when still
      active, a rollback failure adds context, and the original status is
      preserved.
- [x] Link the new source into the `apl_app` target.
- [x] Keep existing service integration tests passing.

## Acceptance checks

- [x] Every service uses `apl_use_case_begin` / `commit` / `rollback`.
- [x] No service has a local `with_context`, `rollback`, or `commit` clone.
- [x] Existing transaction, service, and audit tests pass.
- [x] Formatting and static-analysis checks pass.

## Boundaries

- This item does not introduce a callback-based use-case framework or hide the
  transaction type. Services keep their explicit repository calls.
- The audit-diff builder remains a separate follow-up; it is not required to
  consolidate transaction cleanup.
