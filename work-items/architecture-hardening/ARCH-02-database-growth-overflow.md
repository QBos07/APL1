# ARCH-02 — Shared array growth and overflow checks

**Status:** Implemented  
**Target:** Before work item 4.3

## Goal

Make repository list growth consistent and reject capacity/byte-size overflow
instead of relying on unsigned wrap-around.

## Work list

- [x] Add `apl_grow_capacity` and `apl_grow_array` to `src/core/util.h`.
- [x] Use C23 `<stdckdint.h>` to check both capacity doubling and byte size.
- [x] Replace the repeated `realloc` blocks in activity, car, driver,
      membership, season, and team repositories.
- [x] Remove the one-off team capacity check now covered by the shared helper.
- [x] Add unit tests for capacity growth, overflow rejection, and allocation.

## Acceptance checks

- [x] Every repository list uses `apl_grow_array`.
- [x] A capacity or byte-size overflow returns allocation failure instead of
      wrapping.
- [x] Existing list/transaction tests pass.
- [x] Formatting and static-analysis checks pass.

## Boundaries

- The helper deliberately exposes `void *` and is used through C23 `typeof` at
  each typed call site.
- It does not attempt to unify row reading or SQL binding; those remain explicit
  in each repository.
