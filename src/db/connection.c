#include "db/connection.h"

/** Enable SQLite foreign-key enforcement and verify it is active.
 *
 * Every application connection enforces declared foreign keys, and setup
 * fails (instead of continuing silently) if enforcement cannot be confirmed.
 */
static apl_status enable_foreign_keys(sqlite3 *const handle, apl_error *const error)
{
    char *sqlite_error = nullptr;
    if (sqlite3_exec(handle, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &sqlite_error) !=
        SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_FOREIGN_KEYS, "cannot enable foreign keys: %s",
                      (sqlite_error != nullptr) ? sqlite_error : "unknown error");
        sqlite3_free(sqlite_error);
        return APL_ERR_DB_FOREIGN_KEYS;
    }
    sqlite3_free(sqlite_error);

    sqlite3_stmt *verification = nullptr;
    if (sqlite3_prepare_v2(handle, "PRAGMA foreign_keys;", -1, &verification, nullptr) !=
        SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_FOREIGN_KEYS, "cannot verify foreign-key enforcement: %s",
                      sqlite3_errmsg(handle));
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

/** Apply operational settings shared by every application connection.
 *
 * A short busy timeout keeps concurrent reads/writes from failing immediately.
 * WAL improves read/write overlap for the single-user desktop workflow, and
 * NORMAL synchronous mode keeps commits durable without an fsync per statement.
 */
static apl_status apply_operational_settings(sqlite3 *const handle, apl_error *const error)
{
    if (sqlite3_busy_timeout(handle, 5000) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_OPEN, "cannot configure database busy timeout: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_OPEN;
    }

    char *sqlite_error = nullptr;
    if (sqlite3_exec(handle, "PRAGMA journal_mode = WAL;", nullptr, nullptr, &sqlite_error) !=
        SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_OPEN, "cannot enable write-ahead logging: %s",
                      (sqlite_error != nullptr) ? sqlite_error : "unknown error");
        sqlite3_free(sqlite_error);
        return APL_ERR_DB_OPEN;
    }
    sqlite3_free(sqlite_error);

    if (sqlite3_exec(handle, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, &sqlite_error) !=
        SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_OPEN, "cannot configure synchronous mode: %s",
                      (sqlite_error != nullptr) ? sqlite_error : "unknown error");
        sqlite3_free(sqlite_error);
        return APL_ERR_DB_OPEN;
    }
    sqlite3_free(sqlite_error);
    return APL_OK;
}

apl_status apl_db_open(apl_db_connection *const connection, const char *const path,
                       apl_error *const error)
{
    if (connection == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "connection pointer is nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (path == nullptr || path[0] == '\0') {
        connection->handle = nullptr;
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database path is empty");
        return APL_ERR_INVALID_ARGUMENT;
    }

    connection->handle = nullptr;

    sqlite3 *handle = nullptr;
    constexpr int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    const int result = sqlite3_open_v2(path, &handle, flags, nullptr);
    if (result != SQLITE_OK) {
        /* Capture the message before releasing the (possibly non-nullptr) handle. */
        const char *const sqlite_message =
            (handle != nullptr) ? sqlite3_errmsg(handle) : sqlite3_errstr(result);
        apl_error_set(error, APL_ERR_DB_OPEN, "cannot open database '%s': %s", path,
                      (sqlite_message != nullptr) ? sqlite_message : "unknown error");
        sqlite3_close(handle);
        connection->handle = nullptr;
        return APL_ERR_DB_OPEN;
    }

    const apl_status key_status = enable_foreign_keys(handle, error);
    if (key_status != APL_OK) {
        /* A connection whose initialization failed must not leak a handle. */
        sqlite3_close(handle);
        connection->handle = nullptr;
        return key_status;
    }
    const apl_status operation_status = apply_operational_settings(handle, error);
    if (operation_status != APL_OK) {
        sqlite3_close(handle);
        connection->handle = nullptr;
        return operation_status;
    }

    connection->handle = handle;
    return APL_OK;
}

void apl_db_close(apl_db_connection *const connection)
{
    if (connection == nullptr || connection->handle == nullptr) {
        return;
    }
    sqlite3_close(connection->handle);
    connection->handle = nullptr;
}

sqlite3 *apl_db_handle(const apl_db_connection *const connection)
{
    if (connection == nullptr) {
        return nullptr;
    }
    return connection->handle;
}
