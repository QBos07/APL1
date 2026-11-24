#ifndef APL_TESTS_INTEGRATION_TEST_SUPPORT_H
#define APL_TESTS_INTEGRATION_TEST_SUPPORT_H

#include <stddef.h>

#include "core/error.h"

/*
 * Shared helpers for SQLite-backed tests. Each test gets its own temporary
 * directory and removes it afterwards, so tests never touch a developer's
 * real database and never change global environment state.
 */

/** Create a unique directory under /tmp by copying a template; `buf` receives
 *  the created directory path and must be at least 32 bytes. */
apl_status apl_test_temp_dir(char *buf, size_t buf_size, apl_error *error);

/** Remove a directory created by apl_test_temp_dir, deleting every file it
 *  contains first (flat directory of files). Succeeds only when the directory
 *  is really gone, so tests fail if they leak files. */
apl_status apl_test_remove_temp_dir(const char *dir, apl_error *error);

#endif /* APL_TESTS_INTEGRATION_TEST_SUPPORT_H */
