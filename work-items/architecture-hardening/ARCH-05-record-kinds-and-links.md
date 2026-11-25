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
- [ ] Decide the cross-feature link model before Phase 5.5/6.7. Prefer explicit
      nullable foreign keys for the known links over a generic polymorphic
      `entity_type/entity_id` table.

## Acceptance checks

- [x] Audit events use stable record-kind codes instead of scattered literals.
- [x] Unknown/missing codes resolve to `APL_RECORD_NONE`.
- [x] Existing history filters and audit tests still pass.
- [ ] Phase 8 global history can route a row to its record.
- [ ] Linked transactions are counted once in feature reports.

## Boundaries

- The link model is deliberately deferred until the concrete Phase 5/6 link
  requirements exist.
- Record kinds are append-only; do not renumber existing values.
