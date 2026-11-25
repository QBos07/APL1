# ARCH-06 — Money and ledger model

**Status:** Partially implemented  
**Target:** Before work item 4.1

## Decision

- Amounts use signed `int64_t` minor units at scale 2 (cents). Floating point is
  never used for money.
- Every value carries a three-letter currency code. v1 uses one reporting
  currency per team; mixed-currency arithmetic is rejected instead of silently
  converted.
- Posted ledger entries are append-oriented. Corrections are reversal or
  adjustment entries that preserve the original entry; do not expose an
  independently editable balance.
- Budget plans are stored separately from actual ledger entries.

## Work list

- [x] Add `src/domain/money.[ch]` with currency normalization, exact parsing,
      overflow-checked addition, and exact formatting.
- [x] Add `APL_FIELD_AMOUNT` / `APL_FIELD_CURRENCY` validation annotations.
- [x] Reject excess precision, exponents, separators, and mixed currencies.
- [x] Add unit tests including `INT64_MIN` formatting and add overflow.
- [ ] Add the transaction/category schema and repository in Phase 4.2/4.3.
- [ ] Encode the opening-balance, income, expense, reversal, and adjustment
      entry kinds in Phase 4.5/4.6/4.10.
- [ ] Store the team reporting currency once the Phase 4.1 policy is finalized.

## Acceptance checks

- [x] Parsing and formatting are exact and overflow-safe.
- [x] Mixed currencies cannot be added.
- [x] Invalid amounts carry `APL_FIELD_AMOUNT`; invalid currencies carry
      `APL_FIELD_CURRENCY`.
- [ ] Phase 4 reports derive balances only from ledger entries.
- [ ] Corrections keep the prior value explainable.

## Boundaries

- This item deliberately does not add tables, screens, or currency conversion.
- If v1 later needs explicit foreign-currency conversion, add a converted
  reporting amount and rate note; do not convert implicitly.
