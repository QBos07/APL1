# ARCH-03 — Structured field validation

**Status:** Implemented  
**Target:** Before work item 4.4

## Goal

Stop using human-readable error text as control flow when deciding which dialog
field should receive focus. Validation errors carry a stable field identifier.

## Work list

- [x] Add `apl_field` and a `field` member to `apl_error`.
- [x] Add `apl_error_set_field` / `apl_error_field_id`; reset the annotation on
      new errors and preserve it when context is prefixed.
- [x] Add `apl_use_case_with_field` for service-level validation annotation.
- [x] Annotate team, season, car, driver, and membership validation failures.
- [x] Replace `strstr()`-based field lookup in the shell with field-ID mapping.
- [x] Fix the driver form mapping so short-name errors focus the short-name field.
- [x] Add unit and integration tests for field annotation and preservation.

## Acceptance checks

- [x] A failed name/label/notes/date/role validation reports the matching
      `apl_field`.
- [x] Adding use-case context preserves the field.
- [x] A new unrelated error clears the previous field.
- [x] Dialog focus no longer depends on message wording.
- [x] Existing service and validation tests pass.
- [x] Formatting and static-analysis checks pass.

## Boundaries

- Field identifiers are form-level, not a general schema for every future
  column. New fields append to `apl_field`.
- This item does not introduce localization or per-field error lists; it only
  replaces string matching with a typed identity.
