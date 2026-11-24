#define _POSIX_C_SOURCE 200809L

#include "db/path.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/** Absolute-directory check used for XDG/HOME candidates. */
static bool is_absolute_path(const char *value)
{
    return value != NULL && value[0] == '/';
}

/** Build a path in `out`; fails with APL_ERR_INVALID_ARGUMENT when it does
 *  not fit instead of silently truncating. */
static apl_status join_path(char *out, size_t out_size, const char *fmt, const char *base,
                            apl_error *error)
{
    const int written = snprintf(out, out_size, fmt, base);
    if (written < 0 || (size_t)written >= out_size) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database path does not fit in %zu bytes",
                      out_size);
        return APL_ERR_INVALID_ARGUMENT;
    }
    return APL_OK;
}

apl_status apl_db_resolve_path_ex(const char *explicit_path,
                                  const char *xdg_data_home,
                                  const char *home,
                                  char *out,
                                  size_t out_size,
                                  apl_error *error)
{
    if (out == NULL || out_size < 2) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database path buffer is not usable");
        return APL_ERR_INVALID_ARGUMENT;
    }

    if (explicit_path != NULL && explicit_path[0] != '\0') {
        return join_path(out, out_size, "%s", explicit_path, error);
    }

    if (is_absolute_path(xdg_data_home)) {
        return join_path(out, out_size, "%s/apl1/apl1.sqlite3", xdg_data_home, error);
    }

    if (is_absolute_path(home)) {
        return join_path(out, out_size, "%s/.local/share/apl1/apl1.sqlite3", home, error);
    }

    apl_error_set(error, APL_ERR_NOT_FOUND,
                  "no database location available: set XDG_DATA_HOME or HOME to an absolute "
                  "path, or configure an explicit database path; the application refuses to "
                  "create a database in the current working directory");
    return APL_ERR_NOT_FOUND;
}

apl_status apl_db_resolve_path(const char *explicit_path, char *out, size_t out_size,
                               apl_error *error)
{
    const char *xdg = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    return apl_db_resolve_path_ex(explicit_path, xdg, home, out, out_size, error);
}

/** Create one directory component unless it already exists as a directory. */
static apl_status ensure_directory(const char *directory, apl_error *error)
{
    struct stat existing;
    if (stat(directory, &existing) == 0) {
        if (S_ISDIR(existing.st_mode)) {
            return APL_OK;
        }
        apl_error_set(error, APL_ERR_IO, "'%s' exists and is not a directory", directory);
        return APL_ERR_IO;
    }
    if (mkdir(directory, 0777) != 0) {
        apl_error_set(error, APL_ERR_IO, "cannot create directory '%s': %s", directory,
                      strerror(errno));
        return APL_ERR_IO;
    }
    return APL_OK;
}

apl_status apl_db_ensure_parent_directory(const char *path, apl_error *error)
{
    if (path == NULL || path[0] == '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database path is empty");
        return APL_ERR_INVALID_ARGUMENT;
    }

    char buffer[APL_DB_PATH_MAX];
    const int written = snprintf(buffer, sizeof(buffer), "%s", path);
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database path does not fit in %d bytes",
                      APL_DB_PATH_MAX);
        return APL_ERR_INVALID_ARGUMENT;
    }

    /* Create every prefix before the final component; the final component is
     * the database file itself and is left to SQLite. */
    for (char *cursor = buffer + 1; *cursor != '\0'; ++cursor) {
        if (*cursor != '/') {
            continue;
        }
        *cursor = '\0';
        const apl_status status = ensure_directory(buffer, error);
        if (status != APL_OK) {
            return status;
        }
        *cursor = '/';
    }
    return APL_OK;
}
