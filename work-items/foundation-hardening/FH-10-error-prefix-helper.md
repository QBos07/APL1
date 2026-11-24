# FH-10 — Move error-context prefixing into `src/core`

**Priority:** Before repository/use-case error chains grow  
**Status:** Done  
**Goal:** Provide one tested way for layers to add context to an existing application error without duplicating buffer handling.

## Work list

- [x] Add a public `apl_error_prefix` (or equivalently named) helper to `src/core/error.h/.c`.
- [x] Preserve the original message before writing into the same fixed buffer; never pass overlapping source/destination to printf formatting.
- [x] Define behavior for a null error pointer, null/empty context, and truncation.
- [x] Use the helper in `src/db/migrations.c` instead of its file-local `prefix_error` implementation.
- [x] Add unit tests for prefix order, preservation of original detail, truncation/NUL termination, and null safety.

## Acceptance checks

- [x] Error context can be added without losing the original detail when the result fits.
- [x] Long combined messages truncate safely according to the existing `APL_ERROR_MESSAGE_MAX` API contract.
- [x] No overlapping-buffer formatting is used.
- [x] Migration behavior/status codes remain unchanged apart from improved consistent context.
- [x] The core helper has no SQLite or Notcurses dependency.

## Implementation notes (2026-11-24)

- Added `apl_error_prefix` to `src/core/error.{h,c}`. It copies the existing
  detail before reformatting (no overlapping source/destination), prefixes the
  context, and relies on `APL_ERROR_MESSAGE_MAX` for safe truncation.
- Defined edge cases: null error is a no-op; null/empty context keeps the
  existing message and updates only the code; an empty existing message becomes
  the context alone.
- Replaced the file-local `prefix_error` in `src/db/migrations.c` with the core
  helper. Migration status codes and behavior are unchanged.
- The helper has no SQLite or Notcurses dependency.

## Verification record (2026-11-24)

- Added seven unit tests for prefix order, context-only, null/empty context, null
  error, truncation/NUL termination, and repeated prefixing.
- Full test suite passes; migration rollback and schema-too-new tests still
  report their expected status codes and contextual messages.

## Dependencies / boundaries

- Depends on completed [1.1](../phase-1/1.1-application-error-type.md).
- The existing 256-byte message size already has a documented/tested truncation guarantee; resizing the error type is not part of this task.
