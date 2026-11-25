#include "db/team_repository.h"

#include <sqlite3.h>
#include <stdlib.h>

#include "core/util.h"
#include "db/sqlite_util.h"

#define TEAM_COLUMNS "id, name, notes, currency, archived, created_at, updated_at, archived_at"

/** Read the current row into a team value. */
static void read_team(sqlite3_stmt *const statement, apl_team *const team)
{
    team->id = sqlite3_column_int64(statement, 0);
    apl_db_copy_column_text(statement, 1, team->name, sizeof(team->name));
    apl_db_copy_column_text(statement, 2, team->notes, sizeof(team->notes));
    apl_db_copy_column_text(statement, 3, team->currency, sizeof(team->currency));
    team->meta.archived = sqlite3_column_int(statement, 4) != 0;
    apl_db_copy_column_text(statement, 5, team->meta.created_at, sizeof(team->meta.created_at));
    apl_db_copy_column_text(statement, 6, team->meta.updated_at, sizeof(team->meta.updated_at));
    apl_db_copy_column_text(statement, 7, team->meta.archived_at, sizeof(team->meta.archived_at));
}

/** Translate a failed team write, including unique-name conflicts. */
static apl_status team_write_error(sqlite3 *const handle, const char *const name,
                                   apl_error *const error)
{
    const int extended = sqlite3_extended_errcode(handle);
    if (extended == SQLITE_CONSTRAINT_UNIQUE || extended == SQLITE_CONSTRAINT_PRIMARYKEY) {
        apl_error_set(error, APL_ERR_CONFLICT, "a team named '%s' already exists", name);
        return APL_ERR_CONFLICT;
    }
    if (extended == SQLITE_CONSTRAINT_CHECK) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "team data violates a stored constraint: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_error_set(error, APL_ERR_DB_QUERY, "team database operation failed: %s",
                  sqlite3_errmsg(handle));
    return APL_ERR_DB_QUERY;
}

/** Prepare, bind text, and execute a write that returns one team row. */
static apl_status write_and_read(sqlite3 *const handle, const char *const sql,
                                 const char *const name, const char *const notes, const int64_t id,
                                 const bool bind_id, apl_team *const out_team,
                                 apl_error *const error)
{
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle, sql, -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare team operation: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }

    int bind_status = SQLITE_OK;
    if (bind_id) {
        bind_status = sqlite3_bind_int64(statement, 1, id);
        if (bind_status == SQLITE_OK) {
            bind_status = sqlite3_bind_text(statement, 2, name, -1, SQLITE_TRANSIENT);
        }
        if (bind_status == SQLITE_OK) {
            bind_status = sqlite3_bind_text(statement, 3, notes, -1, SQLITE_TRANSIENT);
        }
    } else {
        bind_status = sqlite3_bind_text(statement, 1, name, -1, SQLITE_TRANSIENT);
        if (bind_status == SQLITE_OK) {
            bind_status = sqlite3_bind_text(statement, 2, notes, -1, SQLITE_TRANSIENT);
        }
    }

    if (bind_status != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind team operation: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }

    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE && bind_id) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "team %lld was not found", (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        const apl_status status = team_write_error(handle, name, error);
        sqlite3_finalize(statement);
        return status;
    }

    read_team(statement, out_team);
    if (sqlite3_finalize(statement) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot finalize team operation: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    return APL_OK;
}

apl_status apl_team_repository_create(const apl_db_connection *const connection,
                                      const char *const name, const char *const notes,
                                      apl_team *const out_team, apl_error *const error)
{
    if (connection == nullptr || name == nullptr || notes == nullptr || out_team == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "team create arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    *out_team = (apl_team){};
    return write_and_read(handle,
                          "INSERT INTO teams (name, notes, archived, created_at, updated_at) "
                          "VALUES (?1, ?2, 0, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), "
                          "strftime('%Y-%m-%dT%H:%M:%fZ', 'now')) "
                          "RETURNING " TEAM_COLUMNS ";",
                          name, notes, 0, false, out_team, error);
}

apl_status apl_team_repository_get(const apl_db_connection *const connection, const int64_t id,
                                   apl_team *const out_team, apl_error *const error)
{
    if (connection == nullptr || out_team == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "team get arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    *out_team = (apl_team){};
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle, "SELECT " TEAM_COLUMNS " FROM teams WHERE id = ?1;", -1,
                           &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare team lookup: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(statement, 1, id);

    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "team %lld was not found", (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read team: %s", sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }

    read_team(statement, out_team);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_team_repository_update(const apl_db_connection *const connection, const int64_t id,
                                      const char *const name, const char *const notes,
                                      apl_team *const out_team, apl_error *const error)
{
    if (connection == nullptr || name == nullptr || notes == nullptr || out_team == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "team update arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    *out_team = (apl_team){};
    return write_and_read(handle,
                          "UPDATE teams SET name = ?2, notes = ?3, "
                          "updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now') "
                          "WHERE id = ?1 RETURNING " TEAM_COLUMNS ";",
                          name, notes, id, true, out_team, error);
}

apl_status apl_team_repository_archive(const apl_db_connection *const connection, const int64_t id,
                                       apl_team *const out_team, apl_error *const error)
{
    if (connection == nullptr || out_team == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "team archive arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    *out_team = (apl_team){};
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(
            handle,
            "UPDATE teams SET archived = 1, "
            "archived_at = COALESCE(archived_at, strftime('%Y-%m-%dT%H:%M:%fZ', 'now')), "
            "updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now') "
            "WHERE id = ?1 RETURNING " TEAM_COLUMNS ";",
            -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare team archive: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(statement, 1, id);

    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "team %lld was not found", (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot archive team: %s", sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }

    read_team(statement, out_team);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_team_repository_set_currency(const apl_db_connection *const connection,
                                            const int64_t id, const char *const currency,
                                            apl_team *const out_team, apl_error *const error)
{
    if (connection == nullptr || currency == nullptr || out_team == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "team currency arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    *out_team = (apl_team){};
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle,
                           "UPDATE teams SET currency = ?2, "
                           "updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now') "
                           "WHERE id = ?1 RETURNING " TEAM_COLUMNS ";",
                           -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare team currency update: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(statement, 1, id);
    sqlite3_bind_text(statement, 2, currency, -1, SQLITE_TRANSIENT);

    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "team %lld was not found", (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot update team currency: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_team(statement, out_team);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_team_repository_list(const apl_db_connection *const connection,
                                    const bool include_archived, apl_team **const out_teams,
                                    size_t *const out_count, apl_error *const error)
{
    if (connection == nullptr || out_teams == nullptr || out_count == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "team list arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_teams = nullptr;
    *out_count = 0;

    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle,
                           "SELECT " TEAM_COLUMNS " FROM teams "
                           "WHERE (?1 = 1 OR archived = 0) "
                           "ORDER BY archived ASC, name COLLATE NOCASE ASC;",
                           -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare team list: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int(statement, 1, include_archived ? 1 : 0);

    apl_team *teams = nullptr;
    size_t count = 0;
    size_t capacity = 0;
    apl_status status = APL_OK;

    for (;;) {
        const int step_status = sqlite3_step(statement);
        if (step_status == SQLITE_DONE) {
            break;
        }
        if (step_status != SQLITE_ROW) {
            apl_error_set(error, APL_ERR_DB_QUERY, "cannot read team list: %s",
                          sqlite3_errmsg(handle));
            status = APL_ERR_DB_QUERY;
            break;
        }

        if (count == capacity) {
            typeof(teams) const grown = apl_grow_array(teams, &capacity, 8, sizeof(*teams));
            if (grown == nullptr) {
                apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate team list");
                status = APL_ERR_OUT_OF_MEMORY;
                break;
            }
            teams = grown;
        }

        read_team(statement, &teams[count]);
        ++count;
    }

    sqlite3_finalize(statement);
    if (status != APL_OK) {
        free(teams);
        return status;
    }

    *out_teams = teams;
    *out_count = count;
    return APL_OK;
}

void apl_team_repository_free_list(apl_team *const teams)
{
    free(teams);
}
