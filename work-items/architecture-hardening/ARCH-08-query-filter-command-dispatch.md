# ARCH-08 — Query filters and command dispatch

**Status:** Partially implemented  
**Target:** Before work items 8.3–8.14

## Goal

Prepare for section-level search, persisted list position, and complete command
parity with every user-visible window action without building a universal query
language.

## Work list

- [x] Give each tab its own list selection and scroll offset; existing
      per-section filters remain explicit.
- [ ] Add shared query pieces for team scope, date range, text search, order,
      and result counts. Keep per-entity filters explicit.
- [ ] Add indexes for the fields used by Phase 8 filters (team/date/category and
      text-search columns).
- [ ] Move command dispatch toward a handler table keyed by action id, with the
      registry as the source of truth for supported window actions.
- [ ] Keep incomplete commands routed to prefilled forms using the shared form
      definitions from ARCH-07.
- [ ] Inventory all window actions and make every action available through a
      command or the matching dialog handoff; include ledger and budget actions.
- [ ] Add menu/command parity tests for every action in the parity matrix, not
      only one sample per feature.

## Acceptance checks

- [ ] Returning from a detail view restores the filtered list position.
- [ ] Active filters and result counts are visible.
- [ ] Adding a command does not require editing multiple unrelated switch blocks.
- [ ] Query behavior stays scoped to the active team unless all-teams is chosen.
- [ ] No user-visible window action is missing from command help/completion or
      the corresponding dialog handoff.

## Boundaries

- Do not build a generic query language or user-defined filter DSL.
- No saved named views or cross-team reporting in this item.
