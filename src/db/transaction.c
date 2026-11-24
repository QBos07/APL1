#include "db/transaction.h"

#include <stdio.h>
#include <sqlite3.h>

/** Execute one transaction-control statement and translate SQLite errors. */
static apl_status exec_control(sqlite3 *const handle, const char *const sql, const char *const what,
                               apl_error *const error)
{
    char *sqlite_error = nullptr;
    if (sqlite3_exec(handle, sql, nullptr, nullptr, &sqlite_error) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "%s: %s", what,
                      (sqlite_error != nullptr) ? sqlite_error : "unknown SQLite error");
        sqlite3_free(sqlite_error);
        return APL_ERR_DB_QUERY;
    }
    sqlite3_free(sqlite_error);
    return APL_OK;
}

/** Copy a locally collected error into the caller's optional error value. */
static void copy_error(apl_error *const destination, const apl_error *const source)
{
    if (destination == nullptr) {
        return;
    }
    apl_error_set(destination, apl_error_code(source), "%s", apl_error_message(source));
}

apl_status apl_db_transaction_begin(apl_db_transaction *const transaction,
                                    apl_db_connection *const connection, apl_error *const error)
{
    if (transaction == nullptr || connection == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "transaction arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (transaction->active) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "a transaction is already active");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (sqlite3_get_autocommit(handle) == 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "the connection already has an active transaction");
        return APL_ERR_INVALID_ARGUMENT;
    }

    const apl_status status =
        exec_control(handle, "BEGIN IMMEDIATE;", "cannot begin transaction", error);
    if (status != APL_OK) {
        return status;
    }
    transaction->connection = connection;
    transaction->active = true;
    return APL_OK;
}

apl_status apl_db_transaction_commit(apl_db_transaction *const transaction, apl_error *const error)
{
    if (transaction == nullptr || transaction->connection == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "transaction arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (!transaction->active) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "no transaction is active");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3 *const handle = apl_db_handle(transaction->connection);
    if (handle == nullptr) {
        transaction->active = false;
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    apl_error commit_error;
    apl_error_init(&commit_error);
    const apl_status status =
        exec_control(handle, "COMMIT;", "cannot commit transaction", &commit_error);
    if (status == APL_OK) {
        transaction->active = false;
        return APL_OK;
    }

    /* COMMIT can fail while leaving the transaction active (for example when
     * the database is busy). Make a best-effort rollback, but never replace
     * the commit failure with a secondary rollback message. */
    if (sqlite3_get_autocommit(handle) == 0) {
        apl_error rollback_error;
        apl_error_init(&rollback_error);
        if (exec_control(handle, "ROLLBACK;", "cannot roll back after commit failure",
                         &rollback_error) == APL_OK) {
            transaction->active = false;
        } else {
            char context[160];
            snprintf(context, sizeof(context), "rollback after commit failure failed: %s",
                     apl_error_message(&rollback_error));
            apl_error_prefix(&commit_error, APL_ERR_DB_QUERY, context);
        }
    } else {
        transaction->active = false;
    }

    copy_error(error, &commit_error);
    return apl_error_code(&commit_error);
}

apl_status apl_db_transaction_rollback(apl_db_transaction *const transaction,
                                       apl_error *const error)
{
    if (transaction == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "transaction is nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (!transaction->active) {
        return APL_OK; /* idempotent cleanup */
    }
    if (transaction->connection == nullptr) {
        transaction->active = false;
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "transaction connection is nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3 *const handle = apl_db_handle(transaction->connection);
    if (handle == nullptr) {
        transaction->active = false;
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    const apl_status status =
        exec_control(handle, "ROLLBACK;", "cannot roll back transaction", error);
    if (status == APL_OK) {
        transaction->active = false;
        return APL_OK;
    }

    if (sqlite3_get_autocommit(handle) != 0) {
        /* There is no transaction left to roll back (for example a failed
         * COMMIT already rolled it back). Treat cleanup as complete. */
        transaction->active = false;
        apl_error_clear(error);
        return APL_OK;
    }
    return status;
}
