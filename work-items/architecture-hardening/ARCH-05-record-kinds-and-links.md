# ARCH-05 — Record kinds and cross-feature links

**Status:** Partially implemented  
**Target:** Before work items 5.5 and 8.1

## Goal

Give audit/history rows a typed record identity so Phase 8 can open the affected
record, and decide how money, development, and parts records link to each other
without losing referential integrity.

## Work list

- [x] Add `apl_record_kind` with stable storage codes and display labels.
- [x] Use the registry when services append audit events.
- [x] Use the registry for History entity filters.
- [x] Add round-trip and unknown-code tests.
- [x] Add a `apl_record_ref` (`kind` + `id`) and a routing registry that maps a
      kind to a screen/open action.
- [x] Confirm the explicit nullable `transactions.project_id` relationship
      specified in [5.5](../phase-5/5.5-project-transaction-links.md); 5.5
      owns the implementation.
- [ ] Decide and implement the explicit replacement-to-transaction relationship
      before Phase 6.7; do not introduce a generic polymorphic link table for
      these known relationships.
- [x] Register the project record kind in
      [5.1](../phase-5/5.1-project-schema-repository.md).
- [x] Register the update record kind in
      [5.3](../phase-5/5.3-development-update-schema-repository.md).
- [ ] Register parts records in their Phase 6 schema items.

## Acceptance checks

- [x] Audit events use stable record-kind codes instead of scattered literals.
- [x] Unknown/missing codes resolve to `APL_RECORD_NONE`.
- [x] Existing history filters and audit tests still pass.
- [ ] Phase 8 global history can route a row to its record.
- [x] Linked transactions are counted once in feature reports.

## Boundaries

- The concrete Phase 5 project link is specified in 5.5. The Phase 6 replacement
  link still needs a matching explicit-FK decision before implementation.
- Record kinds are append-only; do not renumber existing values.
