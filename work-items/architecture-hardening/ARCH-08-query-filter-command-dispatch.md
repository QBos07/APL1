# ARCH-08 — Query filters and command dispatch

**Status:** Planned  
**Target:** Before work items 8.3–8.8

## Goal

Prepare for section-level search, persisted list position, and many new command
routes without building a universal query language.

## Work list

- [ ] Give each tab its own view state: selection, scroll offset, filters, sort,
      and last detail position.
- [ ] Add shared query pieces for team scope, date range, text search, order,
      and result counts. Keep per-entity filters explicit.
- [ ] Add indexes for the fields used by Phase 8 filters (team/date/category and
      text-search columns).
- [ ] Move command dispatch toward a handler table keyed by action id.
- [ ] Keep incomplete commands routed to prefilled forms using the shared form
      definitions from ARCH-07.
- [ ] Add menu/command parity tests for each new domain route.

## Acceptance checks

- [ ] Returning from a detail view restores the filtered list position.
- [ ] Active filters and result counts are visible.
- [ ] Adding a command does not require editing multiple unrelated switch blocks.
- [ ] Query behavior stays scoped to the active team unless all-teams is chosen.

## Boundaries

- Do not build a generic query language or user-defined filter DSL.
- No saved named views or cross-team reporting in this item.
