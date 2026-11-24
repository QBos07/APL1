#include "db/settings_repository.h"

#include <sqlite3.h>

apl_status apl_settings_repository_get_active_team_id(apl_db_connection *const connection,
                                                      int64_t *const out_team_id,
                                                      apl_error *const error)
{
    if (connection == nullptr || out_team_id == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "settings get arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_team_id = 0;

    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle, "SELECT active_team_id FROM app_settings WHERE id = 1;", -1,
                           &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare active-team lookup: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }

    const int step_status = sqlite3_step(statement);
    if (step_status != SQLITE_ROW) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read active-team setting: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }

    if (sqlite3_column_type(statement, 0) != SQLITE_NULL) {
        *out_team_id = sqlite3_column_int64(statement, 0);
    }
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_settings_repository_set_active_team_id(apl_db_connection *const connection,
                                                      const int64_t team_id, apl_error *const error)
{
    if (connection == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "settings set argument is nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "active team ID must be positive");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(
            handle,
            "UPDATE app_settings SET active_team_id = ?1 "
            "WHERE id = 1 AND EXISTS (SELECT 1 FROM teams WHERE id = ?1 AND archived = 0) "
            "RETURNING active_team_id;",
            -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare active-team update: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(statement, 1, team_id);

    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND,
                      "team %lld is missing or archived and cannot be selected",
                      (long long)team_id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot update active-team setting: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_settings_repository_clear_active_team_id(apl_db_connection *const connection,
                                                        apl_error *const error)
{
    if (connection == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "settings clear argument is nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle,
                           "UPDATE app_settings SET active_team_id = NULL WHERE id = 1 "
                           "RETURNING id;",
                           -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare active-team clear: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }

    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_DB_QUERY, "settings row is missing");
        return APL_ERR_DB_QUERY;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot clear active-team setting: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    sqlite3_finalize(statement);
    return APL_OK;
}
