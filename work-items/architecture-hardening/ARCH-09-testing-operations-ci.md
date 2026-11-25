# ARCH-09 — Testing, operations, and CI

**Status:** Partially implemented  
**Target:** Before work items 9.3–9.6

## Work list

- [x] Add `CMakePresets.json` for Debug, Release, and ASan+UBSan presets.
- [x] Add CI jobs for a Clang build and an ASan+UBSan build.
- [ ] Add scenario fixtures that create a team, ledger, project, part, event,
      and history graph for Phase 9 end-to-end tests.
- [ ] Add controller tests from ARCH-07 so shell behavior is covered without a
      terminal.
- [ ] Review database operational settings: `busy_timeout`, WAL, synchronous
      mode, and backup/restore guidance.
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
