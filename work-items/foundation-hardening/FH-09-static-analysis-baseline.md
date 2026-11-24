# FH-09 — Run a static-analysis baseline

**Priority:** Before or during early Phase 2  
**Status:** Not started  
**Goal:** Use one available C static analyzer to inspect the small foundation codebase and surface actionable issues before it grows.

## Work list

- [ ] Select one initial tool (`cppcheck` or `clang-tidy`) based on availability and C23 support in the development/CI environment.
- [ ] Document a reproducible invocation and the exact source/build configuration analyzed.
- [ ] Run the tool on production C sources, including the variadic error-formatting API and SQLite lifecycle code.
- [ ] Triage every finding as valid, false positive, or deferred; create separate work items for fixes that are not trivial.
- [ ] Add narrow suppressions only when justified and explain each one.
- [ ] Decide whether this is an optional local check or a CI gate after observing noise and runtime.

## Acceptance checks

- [ ] A contributor can reproduce the analysis using the documented command/tool version.
- [ ] Findings and disposition are recorded; no broad “ignore all” suppression hides results.
- [ ] Format-string call sites are checked by both compiler diagnostics and the selected analyzer where supported.
- [ ] No tool is made a mandatory build dependency without an explicit project decision.

## Dependencies / boundaries

- No code feature dependency.
- This task establishes a baseline; substantive findings should be fixed in small follow-up changes.
