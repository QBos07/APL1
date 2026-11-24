#ifndef APL_TESTS_SUPPORT_TEST_SUPPORT_H
#define APL_TESTS_SUPPORT_TEST_SUPPORT_H

#include <stddef.h>

#include "core/error.h"

/*
 * Shared helpers for SQLite-backed tests. Each test gets its own temporary
 * directory and removes it afterwards, so tests never touch a developer's real
 * database and never change global environment state.
 *
 * Temporary-directory policy:
 * - `TMPDIR` is honored only when it is an absolute path naming a writable
 *   directory and leaves room for the per-test suffix within
 *   `APL_TEST_DIR_MAX`. Unset, empty, relative, missing, unwritable, or
 *   over-long values fall back to `/tmp` deterministically. If `/tmp` itself
 *   is unusable, creation fails with a reported `APL_ERR_IO` instead of
 *   inventing another location.
 * - Each test directory is created uniquely beneath that base; no fixed
 *   `/tmp/apl1-test-...` path is constructed.
 * - Cleanup removes the whole tree beneath the unique directory, never follows
 *   symlinks out of it, and reports failures so leaked artifacts fail the test.
 */

/** Maximum path length accepted by the test-directory helpers, including the
 *  terminating NUL. */
constexpr size_t APL_TEST_PATH_MAX = 4096;

/** Maximum length of a directory created beneath the temporary base. Smaller
 *  than APL_TEST_PATH_MAX so joining a child name into a full path buffer
 *  cannot truncate. */
constexpr size_t APL_TEST_DIR_MAX = 2048;

/** Resolve the temporary-directory base from an explicit `TMPDIR` value
 *  (testable variant; never reads the process environment). Always writes an
 *  absolute, usable base on success. */
apl_status apl_test_temp_base_ex(const char *tmpdir, char *out, size_t out_size, apl_error *error);

/** Create a unique directory beneath `base`; `buf` receives the path.
 *
 * `base` must be an absolute path naming an existing directory. The result is
 * NUL-terminated and fits in `buf`. */
apl_status apl_test_temp_dir_in(const char *base, char *buf, size_t buf_size, apl_error *error);

/** Create a unique directory beneath the configured base (see the policy
 *  above). `buf` must be at least `APL_TEST_DIR_MAX` bytes for long TMPDIR
 *  values, though a smaller buffer is accepted when the result fits. */
apl_status apl_test_temp_dir(char *buf, size_t buf_size, apl_error *error);

/** Recursively remove a directory created by one of the functions above.
 *
 * Files, nested directories, and symlink entries are removed; symlink targets
 * outside the directory are never traversed or deleted. Returns `APL_OK` when
 * the directory is already gone. Any other failure is reported, including the
 * path and system error. */
apl_status apl_test_remove_temp_dir(const char *dir, apl_error *error);

#endif /* APL_TESTS_SUPPORT_TEST_SUPPORT_H */
