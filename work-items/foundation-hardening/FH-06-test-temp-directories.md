# FH-06 — Make test temporary-directory helpers portable and recursive

**Priority:** Before Phase 2 repository tests  
**Status:** Done  
**Goal:** Keep SQLite tests isolated while honoring the system temporary-directory configuration and reliably cleaning nested test artifacts.

## Work list

- [x] Choose the base from an absolute, usable `TMPDIR`, with a documented safe fallback when it is unset/invalid.
- [x] Create each test directory uniquely beneath that base; do not construct a fixed `/tmp` path.
- [x] Replace flat-file-only cleanup with recursive removal of files and subdirectories created beneath the test's unique directory.
- [x] Avoid following symlinks outside the test directory during cleanup.
- [x] Report cleanup failures so leaked test artifacts fail the owning test.
- [x] Add tests for custom TMPDIR, unset TMPDIR fallback, nested files/directories, and a symlink entry.

## Acceptance checks

- [x] Tests create their temporary directories beneath the configured TMPDIR when it is valid.
- [x] A nested test directory is removed completely after cleanup.
- [x] Cleanup does not traverse or delete a symlink target outside the temporary root.
- [x] Invalid/unwritable TMPDIR behavior is deterministic and documented.
- [x] Existing database tests remain parallel-safe and leave no temporary files behind.

## Implementation notes (2026-11-24)

- `TMPDIR` is used only when it is absolute and names an existing writable
  directory. Unset, empty, relative, missing, non-directory, or unwritable
  values deterministically fall back to `/tmp`; an unusable `/tmp` is a reported
  `APL_ERR_IO` rather than an invented location.
- Each test directory is created uniquely with `mkdtemp` beneath that base.
- Cleanup walks the tree with `lstat`; symlinks are unlinked rather than
  followed, nested files/directories are removed, and failures report the path
  and system error. Missing directories are an idempotent success.
- Added testable `apl_test_temp_base_ex`, `apl_test_temp_dir_in`, and the
  environment-reading `apl_test_temp_dir` wrapper.

## Verification record (2026-11-24)

- New `tests/integration/test_test_support.c` (7 tests) covers a custom TMPDIR,
  unset/empty/relative/missing/non-directory fallback, unique directories,
  nested cleanup, symlink safety, and repeated cleanup.
- All support tests pass and a full run leaves no `apl1-test-*` directory
  behind; running with `TMPDIR=/tmp` is clean.

## Dependencies / boundaries

- Depends on [FH-05](FH-05-test-support-library.md) if the helper is moved first; otherwise can be implemented in the existing location and moved with it.
- Keep the helper test-only; do not reuse it as production filesystem code.
