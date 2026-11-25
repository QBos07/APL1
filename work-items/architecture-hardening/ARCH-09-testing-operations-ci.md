# ARCH-09 — Testing, operations, and CI

**Status:** Partially implemented  
**Target:** Before work items 9.3–9.6

## Work list

- [x] Add `CMakePresets.json` for Debug, Release, and ASan+UBSan presets.
- [x] Add CI jobs for a Clang build and an ASan+UBSan build.
- [x] Add a reusable scenario fixture for the implemented team/ledger/budget
      graph; extend it with projects, parts, and events when those phases land.
- [x] Add controller tests from ARCH-07 so shell behavior is covered without a
      terminal.
- [x] Configure database operational settings: a 5-second `busy_timeout`, WAL,
      and `synchronous = NORMAL`, with integration coverage.
- [ ] Add a repeatable corruption/locked-database error-path test where
      practical.

## Acceptance checks

- [x] `cmake --preset dev`, `release`, and `asan` configure/build/test.
- [x] CI builds with Clang and with sanitizers.
- [ ] Phase 9 workflows can be expressed with a small scenario builder.
- [ ] Operational failures report actionable messages without partial writes.

## Boundaries

- Backup automation and release-to-release database portability remain deferred
  until the MVP is stable.
