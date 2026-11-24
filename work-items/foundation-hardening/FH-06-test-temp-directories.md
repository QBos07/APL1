# FH-06 — Make test temporary-directory helpers portable and recursive

**Priority:** Before Phase 2 repository tests  
**Status:** Not started  
**Goal:** Keep SQLite tests isolated while honoring the system temporary-directory configuration and reliably cleaning nested test artifacts.

## Work list

- [ ] Choose the base from an absolute, usable `TMPDIR`, with a documented safe fallback when it is unset/invalid.
- [ ] Create each test directory uniquely beneath that base; do not construct a fixed `/tmp` path.
- [ ] Replace flat-file-only cleanup with recursive removal of files and subdirectories created beneath the test's unique directory.
- [ ] Avoid following symlinks outside the test directory during cleanup.
- [ ] Report cleanup failures so leaked test artifacts fail the owning test.
- [ ] Add tests for custom TMPDIR, unset TMPDIR fallback, nested files/directories, and a symlink entry.

## Acceptance checks

- [ ] Tests create their temporary directories beneath the configured TMPDIR when it is valid.
- [ ] A nested test directory is removed completely after cleanup.
- [ ] Cleanup does not traverse or delete a symlink target outside the temporary root.
- [ ] Invalid/unwritable TMPDIR behavior is deterministic and documented.
- [ ] Existing database tests remain parallel-safe and leave no temporary files behind.

## Dependencies / boundaries

- Depends on [FH-05](FH-05-test-support-library.md) if the helper is moved first; otherwise can be implemented in the existing location and moved with it.
- Keep the helper test-only; do not reuse it as production filesystem code.
