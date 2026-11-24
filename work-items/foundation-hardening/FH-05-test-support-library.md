# FH-05 — Move test helpers into a test-only support target

**Priority:** Before adding Phase 2 repositories  
**Status:** Done  
**Goal:** Prevent `apl_db` from publicly exposing integration-test headers or depending on test code.

## Work list

- [x] Move `tests/integration/test_support.c` and `.h` to `tests/support/`.
- [x] Add a small `apl_test_support` static CMake target for those helpers.
- [x] Give the support target private access to production headers and link its required production dependencies explicitly.
- [x] Link `apl1_tests` to the support target and include its headers privately on that test target only.
- [x] Remove `tests/integration` from `apl_db`'s public include directories; keep `apl_db`'s public include surface limited to production headers.
- [x] Update test-support include paths and the contributing layout note.

## Acceptance checks

- [x] `apl_db` builds and links without any include path under `tests/`.
- [x] Only test targets can include `test_support.h`.
- [x] Existing integration tests compile and pass through the new support target.
- [x] The application target does not link `apl_test_support`.
- [x] CMake include visibility is explicit and `apl_db` exposes only the intended production interface.

## Implementation notes (2026-11-24)

- Moved the helpers to `tests/support/test_support.{c,h}`.
- Added the `apl_test_support` static target with `tests/support` as its public
  include directory and private access to `src` plus `apl_core`.
- `apl1_tests` links the support target; application targets do not.
- Removed `tests/integration` from `apl_db`'s public include directories and
  from the test target's include paths. `test_support.h` is reachable only
  through the support target.
- Updated CONTRIBUTING and the test READMEs.

## Verification record (2026-11-24)

- Clean configure/build/test passes.
- `apl_db` now publishes only `src`; no production target has an include path under `tests/`.
- The application target links `apl_app`, `apl_tui`, and Notcurses only; it does
  not link `apl_test_support`.

## Dependencies / boundaries

- Can be done before Phase 2; strongly recommended before adding more repository tests.
- Do not refactor test logic or split the Criterion binaries as part of this cleanup.
