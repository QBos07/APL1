# FH-10 — Move error-context prefixing into `src/core`

**Priority:** Before repository/use-case error chains grow  
**Status:** Not started  
**Goal:** Provide one tested way for layers to add context to an existing application error without duplicating buffer handling.

## Work list

- [ ] Add a public `apl_error_prefix` (or equivalently named) helper to `src/core/error.h/.c`.
- [ ] Preserve the original message before writing into the same fixed buffer; never pass overlapping source/destination to printf formatting.
- [ ] Define behavior for a null error pointer, null/empty context, and truncation.
- [ ] Use the helper in `src/db/migrations.c` instead of its file-local `prefix_error` implementation.
- [ ] Add unit tests for prefix order, preservation of original detail, truncation/NUL termination, and null safety.

## Acceptance checks

- [ ] Error context can be added without losing the original detail when the result fits.
- [ ] Long combined messages truncate safely according to the existing `APL_ERROR_MESSAGE_MAX` API contract.
- [ ] No overlapping-buffer formatting is used.
- [ ] Migration behavior/status codes remain unchanged apart from improved consistent context.
- [ ] The core helper has no SQLite or Notcurses dependency.

## Dependencies / boundaries

- Depends on completed [1.1](../phase-1/1.1-application-error-type.md).
- The existing 256-byte message size already has a documented/tested truncation guarantee; resizing the error type is not part of this task.
