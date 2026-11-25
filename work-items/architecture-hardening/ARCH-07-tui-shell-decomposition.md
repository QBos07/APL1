# ARCH-07 — TUI shell decomposition

**Status:** Planned  
**Target:** Before work item 4.8 (Money screens)

## Why

`src/tui/shell.c` is ~4.7k lines with ~150 static functions and owns navigation,
list state, forms, dialogs, mouse routing, service calls, commands, and
rendering. Phase 4 alone adds Money and Budget screens; Phases 5–7 add
Development, Parts, Events, and Outcome screens. Continuing to extend one file
will make review, testing, and regressions worse.

## Slice order

1. Extract shell state and loading/refresh transitions into
   `tui/shell_state.[ch]`; keep rendering callbacks out of it.
2. Extract a controller event path (`key`, `mouse`, `command`) that mutates state
   without requiring Notcurses. Tests can drive this headlessly.
3. Extract shared form/dialog flows for create/edit/archive with callbacks for
   the feature-specific save operation.
4. Extract per-screen modules:
   - `screen_team.c`
   - `screen_people.c`
   - `screen_seasons.c`
   - `screen_cars.c`
   - `screen_history.c`
   - later `screen_money.c`, `screen_development.c`, `screen_parts.c`,
     `screen_events.c`
5. Leave `shell.c` as the event loop, screen dispatch, and Notcurses bridge.

## Acceptance checks

- [ ] `shell.c` no longer contains per-feature draw/load/form blocks.
- [ ] Controller state transitions are tested without a terminal.
- [ ] Existing TUI smoke behavior and keyboard/mouse parity remain intact.
- [ ] Adding one new screen requires touching its own module plus a dispatch
      entry, not the whole shell.

## Boundaries

- Do not introduce a function-pointer framework before the first two slices
  prove a common screen interface.
- Preserve the existing frame/dialog/list models; this is extraction, not a
  visual redesign.
