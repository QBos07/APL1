#include "db/connection.h"

#include <stdio.h>

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
