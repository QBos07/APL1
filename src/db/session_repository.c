#include "db/session_repository.h"

#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>

#include "core/util.h"
#include "db/sqlite_util.h"

#define SESSION_TEAM "(SELECT team_id FROM events WHERE events.id=event_sessions.event_id)"
#define SESSION_FIELDS "event_id,label,created_at,updated_at"
#define SESSION_COLUMNS "id,team_id," SESSION_FIELDS
#define SESSION_RETURNING "id," SESSION_TEAM "," SESSION_FIELDS

static void read_session(sqlite3_stmt *const s, apl_event_session *const session)
{
    *session = (apl_event_session){};
    session->id = sqlite3_column_int64(s, 0);
    session->team_id = sqlite3_column_int64(s, 1);
    session->event_id = sqlite3_column_int64(s, 2);
    apl_db_copy_column_text(s, 3, session->label, sizeof(session->label));
    apl_db_copy_column_text(s, 4, session->created_at, sizeof(session->created_at));
    apl_db_copy_column_text(s, 5, session->updated_at, sizeof(session->updated_at));
}

static apl_status write_session(const apl_db_connection *const connection, const int64_t team_id,
                                const int64_t event_id, const int64_t id, const char *const label,
                                apl_event_session *const out, apl_error *const error)
{
    char checked[APL_REFERENCE_LABEL_MAX];
    if (!connection || !apl_db_handle(connection) || !out || !label || team_id <= 0 ||
        event_id <= 0 || id < 0 || strlen(label) >= sizeof(checked)) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid session arguments");
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (apl_reference_label_normalize(label, "session label", checked, sizeof(checked), error) !=
        APL_OK) {
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const db = apl_db_handle(connection);
    const char *const sql =
        id == 0
            ? "INSERT INTO event_sessions(event_id,label,created_at,updated_at) "
              "SELECT ?2,?4,strftime('%Y-%m-%dT%H:%M:%fZ','now'),"
              "strftime('%Y-%m-%dT%H:%M:%fZ','now') "
              "WHERE EXISTS (SELECT 1 FROM events WHERE id=?2 AND team_id=?1) "
              "RETURNING " SESSION_RETURNING
            : "UPDATE event_sessions SET label=?4,updated_at=strftime('%Y-%m-%dT%H:%M:%fZ','now') "
              "WHERE " SESSION_TEAM "=?1 AND event_id=?2 AND id=?3 RETURNING " SESSION_RETURNING;
    sqlite3_stmt *s = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &s, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare session write: %s",
                      sqlite3_errmsg(db));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(s, 1, team_id);
    sqlite3_bind_int64(s, 2, event_id);
    sqlite3_bind_int64(s, 3, id);
    sqlite3_bind_text(s, 4, label, -1, SQLITE_TRANSIENT);
    const int rc = sqlite3_step(s);
    apl_status status = APL_OK;
    if (rc == SQLITE_ROW) {
        read_session(s, out);
    } else if (rc == SQLITE_DONE) {
        status = id ? APL_ERR_NOT_FOUND : APL_ERR_INVALID_ARGUMENT;
        apl_error_set(error, status, "session not found for this event and team");
    } else {
        status = (rc & 0xff) == SQLITE_CONSTRAINT ? APL_ERR_INVALID_ARGUMENT : APL_ERR_DB_QUERY;
        apl_error_set(error, status, "cannot save session: %s", sqlite3_errmsg(db));
    }
    sqlite3_finalize(s);
    return status;
}

apl_status apl_session_repository_create(const apl_db_connection *const connection,
                                         const int64_t team_id, const int64_t event_id,
                                         const char *const label, apl_event_session *const out,
                                         apl_error *const error)
{
    return write_session(connection, team_id, event_id, 0, label, out, error);
}

apl_status apl_session_repository_update(const apl_db_connection *const connection,
                                         const int64_t team_id, const int64_t event_id,
                                         const int64_t id, const char *const label,
                                         apl_event_session *const out, apl_error *const error)
{
    if (id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid session id");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return write_session(connection, team_id, event_id, id, label, out, error);
}

apl_status apl_session_repository_get(const apl_db_connection *const connection,
                                      const int64_t team_id, const int64_t event_id,
                                      const int64_t id, apl_event_session *const out,
                                      apl_error *const error)
{
    if (!connection || !apl_db_handle(connection) || team_id <= 0 || event_id <= 0 || id <= 0 ||
        !out) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid session lookup arguments");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const db = apl_db_handle(connection);
    sqlite3_stmt *s = nullptr;
    if (sqlite3_prepare_v2(db,
                           "SELECT " SESSION_COLUMNS " FROM event_sessions_scoped "
                           "WHERE team_id=?1 AND event_id=?2 AND id=?3",
                           -1, &s, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare session lookup: %s",
                      sqlite3_errmsg(db));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(s, 1, team_id);
    sqlite3_bind_int64(s, 2, event_id);
    sqlite3_bind_int64(s, 3, id);
    const int rc = sqlite3_step(s);
    apl_status status = APL_OK;
    if (rc == SQLITE_ROW) {
        read_session(s, out);
    } else {
        status = rc == SQLITE_DONE ? APL_ERR_NOT_FOUND : APL_ERR_DB_QUERY;
        apl_error_set(error, status, "session lookup failed: %s", sqlite3_errmsg(db));
    }
    sqlite3_finalize(s);
    return status;
}

apl_status apl_session_repository_list(const apl_db_connection *const connection,
                                       const int64_t team_id, const int64_t event_id,
                                       apl_event_session **const out, size_t *const count,
                                       apl_error *const error)
{
    if (!connection || !apl_db_handle(connection) || team_id <= 0 || event_id <= 0 || !out ||
        !count) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid session list arguments");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out = nullptr;
    *count = 0;
    sqlite3 *const db = apl_db_handle(connection);
    sqlite3_stmt *s = nullptr;
    if (sqlite3_prepare_v2(db,
                           "SELECT " SESSION_COLUMNS " FROM event_sessions_scoped "
                           "WHERE team_id=?1 AND event_id=?2 ORDER BY id",
                           -1, &s, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare session list: %s",
                      sqlite3_errmsg(db));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(s, 1, team_id);
    sqlite3_bind_int64(s, 2, event_id);
    apl_event_session *rows = nullptr;
    size_t capacity = 0;
    int rc;
    while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
        if (*count == capacity) {
            apl_event_session *const grown = apl_grow_array(rows, &capacity, 8, sizeof(*rows));
            if (!grown) {
                free(rows);
                sqlite3_finalize(s);
                *count = 0;
                apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate session list");
                return APL_ERR_OUT_OF_MEMORY;
            }
            rows = grown;
        }
        read_session(s, &rows[(*count)++]);
    }
    if (rc != SQLITE_DONE) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read session list: %s", sqlite3_errmsg(db));
        free(rows);
        *count = 0;
        sqlite3_finalize(s);
        return APL_ERR_DB_QUERY;
    }
    sqlite3_finalize(s);
    *out = rows;
    return APL_OK;
}

void apl_session_repository_free_list(apl_event_session *const sessions)
{
    free(sessions);
}
