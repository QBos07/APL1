#ifndef APL_DB_PATH_H
#define APL_DB_PATH_H

#include <stddef.h>

#include "core/error.h"

/** Maximum path length accepted by the database path module, including the
 *  terminating NUL. Longer paths are rejected instead of truncated. */
constexpr size_t APL_DB_PATH_MAX = 4096;

/*
 * Database path policy (v1, confirmed from the 1.3 work item):
 *
 * 1. A path explicitly supplied through application configuration wins.
 * 2. Otherwise `$XDG_DATA_HOME/apl1/apl1.sqlite3` when XDG_DATA_HOME is set,
 *    non-empty, and absolute (relative values violate the XDG spec and are
 *    ignored).
 * 3. Otherwise `$HOME/.local/share/apl1/apl1.sqlite3` when HOME is set,
 *    non-empty, and absolute.
 * 4. Otherwise the resolver fails with a clear error; there is deliberately
 *    no fallback that would create a database in the current working
 *    directory.
 *
 * Directory creation only happens for environment-derived defaults, via
 * apl_db_ensure_parent_directory; explicit paths must already have a usable
 * parent directory so that typos are not silently masked.
 */

/** Resolve the database path from explicit inputs (testable variant that
 *  never touches process-global environment variables).
 *
 * `out` must be at least APL_DB_PATH_MAX bytes; the result is always
 * NUL-terminated. On failure `out` is not written. */
apl_status apl_db_resolve_path_ex(const char *explicit_path, const char *xdg_data_home,
                                  const char *home, char *out, size_t out_size, apl_error *error);

/** Production convenience wrapper: resolves per the policy above using
 *  XDG_DATA_HOME and HOME from the environment. */
apl_status apl_db_resolve_path(const char *explicit_path, char *out, size_t out_size,
                               apl_error *error);

/** Create missing parent directories of `path` (mkdir -p behavior for every
 *  component before the final component, which is treated as a file). Used for
 *  environment-derived default paths. Permission and path errors are reported
 *  clearly and never leave a partial database file. */
apl_status apl_db_ensure_parent_directory(const char *path, apl_error *error);

#endif /* APL_DB_PATH_H */
