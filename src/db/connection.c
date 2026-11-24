#include "db/connection.h"

#include <stdio.h>

/** Enable SQLite foreign-key enforcement and verify it is active.
 *
 * Every application connection enforces declared foreign keys, and setup
 * fails (instead of continuing silently) if enforcement cannot be confirmed.
 */
static apl_status enable_foreign_keys(sqlite3 *handle, apl_error *error)
{
    char *sqlite_error = NULL;
    if (sqlite3_exec(handle, "PRAGMA foreign_keys = ON;", NULL, NULL, &sqlite_error) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_FOREIGN_KEYS, "cannot enable foreign keys: %s",
                      (sqlite_error != NULL) ? sqlite_error : "unknown error");
        sqlite3_free(sqlite_error);
        return APL_ERR_DB_FOREIGN_KEYS;
    }
    sqlite3_free(sqlite_error);

    sqlite3_stmt *verification = NULL;
    if (sqlite3_prepare_v2(handle, "PRAGMA foreign_keys;", -1, &verification, NULL) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_FOREIGN_KEYS,
                      "cannot verify foreign-key enforcement: %s", sqlite3_errmsg(handle));
        return APL_ERR_DB_FOREIGN_KEYS;
    }

    int enabled = 0;
    if (sqlite3_step(verification) == SQLITE_ROW) {
        enabled = sqlite3_column_int(verification, 0);
    }
    sqlite3_finalize(verification);

    if (enabled != 1) {
        apl_error_set(error, APL_ERR_DB_FOREIGN_KEYS,
                      "foreign-key enforcement is not active after initialization");
        return APL_ERR_DB_FOREIGN_KEYS;
    }
    return APL_OK;
}

apl_status apl_db_open(apl_db_connection *connection, const char *path, apl_error *error)
{
    if (connection == NULL) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "connection pointer is NULL");
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (path == NULL || path[0] == '\0') {
        connection->handle = NULL;
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database path is empty");
        return APL_ERR_INVALID_ARGUMENT;
    }

    connection->handle = NULL;

    sqlite3 *handle = NULL;
    const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    const int result = sqlite3_open_v2(path, &handle, flags, NULL);
    if (result != SQLITE_OK) {
        /* Capture the message before releasing the (possibly non-NULL) handle. */
        const char *sqlite_message =
                (handle != NULL) ? sqlite3_errmsg(handle) : sqlite3_errstr(result);
        apl_error_set(error, APL_ERR_DB_OPEN, "cannot open database '%s': %s",
                      path, (sqlite_message != NULL) ? sqlite_message : "unknown error");
        sqlite3_close(handle);
        connection->handle = NULL;
        return APL_ERR_DB_OPEN;
    }

    const apl_status key_status = enable_foreign_keys(handle, error);
    if (key_status != APL_OK) {
        /* A connection whose initialization failed must not leak a handle. */
        sqlite3_close(handle);
        connection->handle = NULL;
        return key_status;
    }

    connection->handle = handle;
    return APL_OK;
}

void apl_db_close(apl_db_connection *connection)
{
    if (connection == NULL || connection->handle == NULL) {
        return;
    }
    sqlite3_close(connection->handle);
    connection->handle = NULL;
}

sqlite3 *apl_db_handle(apl_db_connection *connection)
{
    if (connection == NULL) {
        return NULL;
    }
    return connection->handle;
}
