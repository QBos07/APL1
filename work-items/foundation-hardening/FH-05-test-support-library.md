# FH-05 — Move test helpers into a test-only support target

**Priority:** Before adding Phase 2 repositories  
**Status:** Not started  
**Goal:** Prevent `apl_db` from publicly exposing integration-test headers or depending on test code.

## Work list

- [ ] Move `tests/integration/test_support.c` and `.h` to `tests/support/`.
- [ ] Add a small `apl_test_support` static CMake target for those helpers.
- [ ] Give the support target private access to production headers and link its required production dependencies explicitly.
- [ ] Link `apl1_tests` to the support target and include its headers privately on that test target only.
- [ ] Remove `tests/integration` from `apl_db`'s public include directories; keep `apl_db`'s public include surface limited to production headers.
- [ ] Update test-support include paths and the contributing layout note.

## Acceptance checks

- [ ] `apl_db` builds and links without any include path under `tests/`.
- [ ] Only test targets can include `test_support.h`.
- [ ] Existing integration tests compile and pass through the new support target.
- [ ] The application target does not link `apl_test_support`.
- [ ] CMake include visibility is explicit and `apl_db` exposes only the intended production interface.

## Dependencies / boundaries

- Can be done before Phase 2; strongly recommended before adding more repository tests.
- Do not refactor test logic or split the Criterion binaries as part of this cleanup.
