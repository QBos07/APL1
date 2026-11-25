#include "db/season_repository.h"

#include <sqlite3.h>
#include <stdlib.h>

#include "db/sqlite_util.h"

#define SEASON_COLUMNS                                                                             \
    "id, team_id, label, start_date, end_date, notes, archived, created_at, updated_at, archived_at"

static void bind_optional_text(sqlite3_stmt *const statement, const int index,
                               const char *const value)
{
    if (value == nullptr || value[0] == '\0') {
        (void)sqlite3_bind_null(statement, index);
    } else {
        (void)sqlite3_bind_text(statement, index, value, -1, SQLITE_TRANSIENT);
    }
}

static void read_season(sqlite3_stmt *const statement, apl_season *const season)
{
    season->id = sqlite3_column_int64(statement, 0);
    season->team_id = sqlite3_column_int64(statement, 1);
    apl_db_copy_column_text(statement, 2, season->label, sizeof(season->label));
    apl_db_copy_column_text(statement, 3, season->start_date, sizeof(season->start_date));
    apl_db_copy_column_text(statement, 4, season->end_date, sizeof(season->end_date));
    apl_db_copy_column_text(statement, 5, season->notes, sizeof(season->notes));
    season->archived = sqlite3_column_int(statement, 6) != 0;
    apl_db_copy_column_text(statement, 7, season->created_at, sizeof(season->created_at));
    apl_db_copy_column_text(statement, 8, season->updated_at, sizeof(season->updated_at));
    apl_db_copy_column_text(statement, 9, season->archived_at, sizeof(season->archived_at));
}

static apl_status write_error(sqlite3 *const handle, const char *const label,
                              apl_error *const error)
{
    const int extended = sqlite3_extended_errcode(handle);
    if (extended == SQLITE_CONSTRAINT_UNIQUE || extended == SQLITE_CONSTRAINT_PRIMARYKEY) {
        apl_error_set(error, APL_ERR_CONFLICT,
                      "a season labelled '%s' already exists for this team", label);
        return APL_ERR_CONFLICT;
    }
    if (extended == SQLITE_CONSTRAINT_FOREIGNKEY) {
        apl_error_set(error, APL_ERR_NOT_FOUND, "the owning team no longer exists");
        return APL_ERR_NOT_FOUND;
    }
    if (extended == SQLITE_CONSTRAINT_CHECK) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "season data violates a stored constraint: %s", sqlite3_errmsg(handle));
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_error_set(error, APL_ERR_DB_QUERY, "season database operation failed: %s",
                  sqlite3_errmsg(handle));
    return APL_ERR_DB_QUERY;
}

static apl_status write_and_read(sqlite3 *const handle, const char *const sql,
                                 const int64_t team_id, const int64_t id, const bool bind_id,
                                 const char *const label, const char *const start_date,
                                 const char *const end_date, const char *const notes,
                                 apl_season *const out_season, apl_error *const error)
{
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle, sql, -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare season operation: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }

    int bind_status = sqlite3_bind_int64(statement, 1, team_id);
    int next = 2;
    if (bind_id) {
        if (bind_status == SQLITE_OK) {
            bind_status = sqlite3_bind_int64(statement, next++, id);
        }
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, next++, label, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_optional_text(statement, next++, start_date);
    }
    if (bind_status == SQLITE_OK) {
        bind_optional_text(statement, next++, end_date);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, next, notes, -1, SQLITE_TRANSIENT);
    }

    if (bind_status != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind season operation: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }

    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE && bind_id) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "season %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        const apl_status status = write_error(handle, label, error);
        sqlite3_finalize(statement);
        return status;
    }
    read_season(statement, out_season);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_season_repository_create(const apl_db_connection *const connection,
                                        const int64_t team_id, const char *const label,
                                        const char *const start_date, const char *const end_date,
                                        const char *const notes, apl_season *const out_season,
                                        apl_error *const error)
{
    if (connection == nullptr || label == nullptr || start_date == nullptr || end_date == nullptr ||
        notes == nullptr || out_season == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "season create arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_season = (apl_season){};
    return write_and_read(
        handle,
        "INSERT INTO seasons (team_id, label, start_date, end_date, notes, archived, created_at, "
        "updated_at) VALUES (?1, ?2, ?3, ?4, ?5, 0, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), "
        "strftime('%Y-%m-%dT%H:%M:%fZ', 'now')) RETURNING " SEASON_COLUMNS ";",
        team_id, 0, false, label, start_date, end_date, notes, out_season, error);
}

apl_status apl_season_repository_get(const apl_db_connection *const connection,
                                     const int64_t team_id, const int64_t id,
                                     apl_season *const out_season, apl_error *const error)
{
    if (connection == nullptr || out_season == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "season get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_season = (apl_season){};

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle,
                           "SELECT " SEASON_COLUMNS " FROM seasons WHERE id = ?1 AND team_id = ?2;",
                           -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare season lookup: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(statement, 1, id);
    sqlite3_bind_int64(statement, 2, team_id);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "season %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read season: %s", sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_season(statement, out_season);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_season_repository_update(const apl_db_connection *const connection,
                                        const int64_t team_id, const int64_t id,
                                        const char *const label, const char *const start_date,
                                        const char *const end_date, const char *const notes,
                                        apl_season *const out_season, apl_error *const error)
{
    if (connection == nullptr || label == nullptr || start_date == nullptr || end_date == nullptr ||
        notes == nullptr || out_season == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "season update arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_season = (apl_season){};
    return write_and_read(
        handle,
        "UPDATE seasons SET label = ?3, start_date = ?4, end_date = ?5, notes = ?6, "
        "updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now') "
        "WHERE id = ?2 AND team_id = ?1 RETURNING " SEASON_COLUMNS ";",
        team_id, id, true, label, start_date, end_date, notes, out_season, error);
}

apl_status apl_season_repository_archive(const apl_db_connection *const connection,
                                         const int64_t team_id, const int64_t id,
                                         apl_season *const out_season, apl_error *const error)
{
    if (connection == nullptr || out_season == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "season archive arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_season = (apl_season){};

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(
            handle,
            "UPDATE seasons SET archived = 1, "
            "archived_at = COALESCE(archived_at, strftime('%Y-%m-%dT%H:%M:%fZ', 'now')), "
            "updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now') "
            "WHERE id = ?2 AND team_id = ?1 RETURNING " SEASON_COLUMNS ";",
            -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare season archive: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(statement, 1, team_id);
    sqlite3_bind_int64(statement, 2, id);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "season %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot archive season: %s", sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_season(statement, out_season);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_season_repository_list(const apl_db_connection *const connection,
                                      const int64_t team_id, const bool include_archived,
                                      apl_season **const out_seasons, size_t *const out_count,
                                      apl_error *const error)
{
    if (connection == nullptr || out_seasons == nullptr || out_count == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "season list arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_seasons = nullptr;
    *out_count = 0;
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle,
                           "SELECT " SEASON_COLUMNS " FROM seasons WHERE team_id = ?1 "
                           "AND (?2 = 1 OR archived = 0) "
                           "ORDER BY archived ASC, label COLLATE NOCASE ASC, id ASC;",
                           -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare season list: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(statement, 1, team_id);
    sqlite3_bind_int(statement, 2, include_archived ? 1 : 0);

    apl_season *seasons = nullptr;
    size_t count = 0;
    size_t capacity = 0;
    apl_status status = APL_OK;
    for (;;) {
        const int step_status = sqlite3_step(statement);
        if (step_status == SQLITE_DONE) {
            break;
        }
        if (step_status != SQLITE_ROW) {
            apl_error_set(error, APL_ERR_DB_QUERY, "cannot read season list: %s",
                          sqlite3_errmsg(handle));
            status = APL_ERR_DB_QUERY;
            break;
        }
        if (count == capacity) {
            const size_t grown_capacity = (capacity == 0) ? 8 : capacity * 2;
            typeof(seasons) const grown = realloc(seasons, grown_capacity * sizeof(*seasons));
            if (grown == nullptr) {
                apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate season list");
                status = APL_ERR_OUT_OF_MEMORY;
                break;
            }
            seasons = grown;
            capacity = grown_capacity;
        }
        read_season(statement, &seasons[count]);
        ++count;
    }
    sqlite3_finalize(statement);
    if (status != APL_OK) {
        free(seasons);
        return status;
    }
    *out_seasons = seasons;
    *out_count = count;
    return APL_OK;
}

void apl_season_repository_free_list(apl_season *const seasons)
{
    free(seasons);
}
