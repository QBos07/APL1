# FH-08 — Adopt and check a C formatter configuration

**Priority:** Before Phase 2 adds many source files  
**Status:** Not started  
**Goal:** Establish a consistent formatting configuration while the codebase is still small.

## Work list

- [ ] Select a project formatting base/style and add a checked-in `.clang-format`.
- [ ] Pin/document the supported clang-format major version or clearly document version tolerance.
- [ ] Format the current maintained C headers/sources and tests with that configuration in a dedicated change.
- [ ] Add a script or documented command that checks formatting without rewriting files.
- [ ] Document formatting check usage in CONTRIBUTING and optionally call it from `scripts/check.sh` only if clang-format is an explicit required dependency.

## Acceptance checks

- [ ] The repository has one configuration usable from the project root.
- [ ] Running the documented check on the formatted files succeeds without modifying them.
- [ ] The check reports nonzero when a controlled formatting violation is introduced (verify locally and revert the fixture).
- [ ] Formatting changes are behavior-neutral and do not mix with feature implementation.
- [ ] The dependency policy for local/CI formatting checks is explicit.

## Dependencies / boundaries

- No dependency on Phase 2 code.
- Do not add a broad CI dependency silently; document or install the formatter intentionally.
