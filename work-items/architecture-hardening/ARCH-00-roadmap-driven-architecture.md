# Roadmap-driven architecture hardening

**Status:** In progress

This folder slices the architectural review performed after Phase 3. The order
follows the roadmap: infrastructure that Phases 4–9 depend on lands first;
larger changes that are only needed by a later phase stay documented with
explicit triggers.

## Sequencing

| Item | Purpose | Target | Status |
| ---- | ------- | ------ | ------ |
| [ARCH-01](ARCH-01-shared-use-case-transaction.md) | Shared write-use-case transaction/error plumbing | Before 4.6 | Implemented |
| [ARCH-02](ARCH-02-database-growth-overflow.md) | Shared array-growth and overflow checks | Before 4.3 | Planned |
| [ARCH-03](ARCH-03-structured-field-validation.md) | Structured validation field identifiers | Before 4.4 | Planned |
| [ARCH-04](ARCH-04-record-metadata-composition.md) | Compose shared record metadata | Before 5.1 | Planned |
| [ARCH-05](ARCH-05-record-kinds-and-links.md) | Typed record references and cross-feature links | Before 5.5 / 8.1 | Planned |
| [ARCH-06](ARCH-06-money-and-ledger-model.md) | Money representation and ledger/correction model | Before 4.1 | Planned |
| [ARCH-07](ARCH-07-tui-shell-decomposition.md) | Split the monolithic shell into screens and a testable controller | Before 4.8 | Planned |
| [ARCH-08](ARCH-08-query-filter-command-dispatch.md) | Per-view filters, query pieces, command handler table | Before 8.3 | Planned |
| [ARCH-09](ARCH-09-testing-operations-ci.md) | Scenario fixtures, sanitizer/Clang CI, database operations | Before 9.3 | Planned |

## Ordering rules

- Phase 4 must not start its money schema before ARCH-06 settles the amount and
  correction model.
- Each new entity in Phases 4–7 should use the shared transaction/audit helpers
  from ARCH-01 instead of copying service boilerplate.
- New screens/forms should use the result of ARCH-07; adding another feature to
  `src/tui/shell.c` directly should be treated as temporary.
- New record types should get an ARCH-05 record-kind entry before they emit
  audit events, so Phase 8 history can open the affected record.
- Advanced filtering, custom fields, inventory, attachments, multi-currency
  conversion, and export remain explicitly deferred by the product roadmap.
