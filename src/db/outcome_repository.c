#include "db/outcome_repository.h"

#include <stdlib.h>
#include <string.h>

#include "core/util.h"
#include "db/sqlite_util.h"

#define OUTCOME_TEAM                                                                               \
    "(SELECT team_id FROM event_sessions_scoped WHERE id=driver_outcomes.session_id)"
#define OUTCOME_EVENT "(SELECT event_id FROM event_sessions WHERE id=driver_outcomes.session_id)"
#define OUTCOME_FIELDS                                                                             \
    "session_id,driver_id,start_position,finish_position,"                                         \
    "laps,status,notes,created_at,updated_at"
#define COLUMNS "id,team_id,event_id," OUTCOME_FIELDS
#define RETURNING_COLUMNS "id," OUTCOME_TEAM "," OUTCOME_EVENT "," OUTCOME_FIELDS

static void read_outcome(sqlite3_stmt *s, apl_outcome *o)
{
    *o = (apl_outcome){};
    o->id = sqlite3_column_int64(s, 0);
    o->team_id = sqlite3_column_int64(s, 1);
    o->event_id = sqlite3_column_int64(s, 2);
    o->session_id = sqlite3_column_int64(s, 3);
    o->driver_id = sqlite3_column_int64(s, 4);
    o->has_start_position = sqlite3_column_type(s, 5) != SQLITE_NULL;
    o->has_finish_position = sqlite3_column_type(s, 6) != SQLITE_NULL;
    o->has_laps = sqlite3_column_type(s, 7) != SQLITE_NULL;
    o->has_status = sqlite3_column_type(s, 8) != SQLITE_NULL;
    o->has_notes = sqlite3_column_type(s, 9) != SQLITE_NULL;
    o->start_position = sqlite3_column_int(s, 5);
    o->finish_position = sqlite3_column_int(s, 6);
    o->laps = sqlite3_column_int(s, 7);
    apl_db_copy_column_text(s, 8, o->status, sizeof(o->status));
    apl_db_copy_column_text(s, 9, o->notes, sizeof(o->notes));
    apl_db_copy_column_text(s, 10, o->created_at, sizeof(o->created_at));
    apl_db_copy_column_text(s, 11, o->updated_at, sizeof(o->updated_at));
}

static apl_status write_outcome(const apl_db_connection *connection, int64_t team_id, int64_t id,
                                const apl_outcome *values, apl_outcome *out, apl_error *error)
{
    if (!connection || !apl_db_handle(connection) || !values || !out || team_id <= 0 || id < 0 ||
        (!id && (values->event_id <= 0 || values->session_id <= 0 || values->driver_id <= 0)) ||
        (values->has_status && !memchr(values->status, '\0', sizeof(values->status))) ||
        (values->has_notes && !memchr(values->notes, '\0', sizeof(values->notes)))) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid outcome arguments");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *db = apl_db_handle(connection);
    const char *sql =
        id ? "UPDATE driver_outcomes SET start_position=?6,finish_position=?7,laps=?8,status=?9,"
             "notes=?10,updated_at=strftime('%Y-%m-%dT%H:%M:%fZ','now') "
             "WHERE " OUTCOME_TEAM "=?1 AND id=?2 RETURNING " RETURNING_COLUMNS
           : "INSERT INTO driver_outcomes(session_id,driver_id,start_position,"
             "finish_position,laps,status,notes,created_at,updated_at) "
             "SELECT ?4,?5,?6,?7,?8,?9,?10,strftime('%Y-%m-%dT%H:%M:%fZ','now'),"
             "strftime('%Y-%m-%dT%H:%M:%fZ','now') WHERE EXISTS "
             "(SELECT 1 FROM event_sessions_scoped WHERE id=?4 AND event_id=?3 AND team_id=?1) "
             "RETURNING " RETURNING_COLUMNS;
    sqlite3_stmt *s = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &s, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare outcome write: %s",
                      sqlite3_errmsg(db));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(s, 1, team_id);
    sqlite3_bind_int64(s, 2, id);
    sqlite3_bind_int64(s, 3, values->event_id);
    sqlite3_bind_int64(s, 4, values->session_id);
    sqlite3_bind_int64(s, 5, values->driver_id);
    if (values->has_start_position)
        sqlite3_bind_int(s, 6, values->start_position);
    if (values->has_finish_position)
        sqlite3_bind_int(s, 7, values->finish_position);
    if (values->has_laps)
        sqlite3_bind_int(s, 8, values->laps);
    if (values->has_status)
        sqlite3_bind_text(s, 9, values->status, -1, SQLITE_TRANSIENT);
    if (values->has_notes)
        sqlite3_bind_text(s, 10, values->notes, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(s);
    apl_status result = APL_OK;
    if (rc == SQLITE_ROW)
        read_outcome(s, out);
    else if (rc == SQLITE_DONE) {
        result = id ? APL_ERR_NOT_FOUND : APL_ERR_INVALID_ARGUMENT;
        apl_error_set(error, result, "outcome not found for this team");
    } else {
        int extended = sqlite3_extended_errcode(db);
        result = extended == SQLITE_CONSTRAINT_UNIQUE ? APL_ERR_CONFLICT
                 : (rc & 0xff) == SQLITE_CONSTRAINT   ? APL_ERR_INVALID_ARGUMENT
                                                      : APL_ERR_DB_QUERY;
        apl_error_set(error, result, "cannot save outcome: %s", sqlite3_errmsg(db));
    }
    sqlite3_finalize(s);
    return result;
}

apl_status apl_outcome_repository_create(const apl_db_connection *connection,
                                         const apl_outcome *input, apl_outcome *out,
                                         apl_error *error)
{
    return write_outcome(connection, input ? input->team_id : 0, 0, input, out, error);
}

apl_status apl_outcome_repository_update(const apl_db_connection *connection, int64_t team_id,
                                         int64_t id, const apl_outcome *values, apl_outcome *out,
                                         apl_error *error)
{
    if (id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid outcome id");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return write_outcome(connection, team_id, id, values, out, error);
}

apl_status apl_outcome_repository_get(const apl_db_connection *connection, int64_t team_id,
                                      int64_t id, apl_outcome *out, apl_error *error)
{
    if (!connection || !apl_db_handle(connection) || team_id <= 0 || id <= 0 || !out) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid outcome lookup arguments");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *db = apl_db_handle(connection);
    sqlite3_stmt *s = nullptr;
    if (sqlite3_prepare_v2(
            db, "SELECT " COLUMNS " FROM driver_outcomes_scoped WHERE team_id=?1 AND id=?2", -1, &s,
            nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare outcome lookup: %s",
                      sqlite3_errmsg(db));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(s, 1, team_id);
    sqlite3_bind_int64(s, 2, id);
    int rc = sqlite3_step(s);
    apl_status status = APL_OK;
    if (rc == SQLITE_ROW)
        read_outcome(s, out);
    else {
        status = rc == SQLITE_DONE ? APL_ERR_NOT_FOUND : APL_ERR_DB_QUERY;
        apl_error_set(error, status, "outcome lookup failed: %s", sqlite3_errmsg(db));
    }
    sqlite3_finalize(s);
    return status;
}

apl_status apl_outcome_repository_list(const apl_db_connection *connection, int64_t team_id,
                                       int64_t event_id, int64_t session_id, apl_outcome **out,
                                       size_t *count, apl_error *error)
{
    if (!connection || !apl_db_handle(connection) || team_id <= 0 || event_id <= 0 ||
        session_id <= 0 || !out || !count) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid outcome list arguments");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out = nullptr;
    *count = 0;
    sqlite3 *db = apl_db_handle(connection);
    sqlite3_stmt *s = nullptr;
    if (sqlite3_prepare_v2(db,
                           "SELECT " COLUMNS " FROM driver_outcomes_scoped WHERE team_id=?1 "
                           "AND event_id=?2 AND session_id=?3 ORDER BY id",
                           -1, &s, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare outcome list: %s",
                      sqlite3_errmsg(db));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(s, 1, team_id);
    sqlite3_bind_int64(s, 2, event_id);
    sqlite3_bind_int64(s, 3, session_id);
    apl_outcome *rows = nullptr;
    size_t capacity = 0;
    int rc;
    while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
        if (*count == capacity) {
            apl_outcome *grown = apl_grow_array(rows, &capacity, 8, sizeof(*rows));
            if (!grown) {
                free(rows);
                sqlite3_finalize(s);
                *count = 0;
                apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate outcome list");
                return APL_ERR_OUT_OF_MEMORY;
            }
            rows = grown;
        }
        read_outcome(s, &rows[(*count)++]);
    }
    if (rc != SQLITE_DONE) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read outcome list: %s", sqlite3_errmsg(db));
        free(rows);
        *count = 0;
        sqlite3_finalize(s);
        return APL_ERR_DB_QUERY;
    }
    sqlite3_finalize(s);
    *out = rows;
    return APL_OK;
}

void apl_outcome_repository_free_list(apl_outcome *outcomes)
{
    free(outcomes);
}
