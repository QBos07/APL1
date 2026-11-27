#include "db/event_repository.h"

#include <stdlib.h>
#include <string.h>

#include "core/util.h"
#include "db/sqlite_util.h"

#define EVENT_COLUMNS "id,team_id,season_id,name,event_date,location,notes,created_at,updated_at"

static void read_event(sqlite3_stmt *s, apl_event *event)
{
    *event = (apl_event){};
    event->id = sqlite3_column_int64(s, 0);
    event->team_id = sqlite3_column_int64(s, 1);
    event->season_id = sqlite3_column_int64(s, 2);
    apl_db_copy_column_text(s, 3, event->name, sizeof(event->name));
    apl_db_copy_column_text(s, 4, event->date, sizeof(event->date));
    apl_db_copy_column_text(s, 5, event->location, sizeof(event->location));
    apl_db_copy_column_text(s, 6, event->notes, sizeof(event->notes));
    apl_db_copy_column_text(s, 7, event->created_at, sizeof(event->created_at));
    apl_db_copy_column_text(s, 8, event->updated_at, sizeof(event->updated_at));
}

static apl_status write_event(const apl_db_connection *connection, int64_t team_id, int64_t id,
                              int64_t season_id, const char *name, const char *date,
                              const char *location, const char *notes, apl_event *out,
                              apl_error *error)
{
    char checked_name[APL_REFERENCE_LABEL_MAX], checked_date[APL_REFERENCE_DATE_MAX];
    if (!connection || !out || team_id <= 0 || id < 0 || season_id < 0 || !name || !date ||
        !location || !notes || strlen(name) >= sizeof(checked_name) ||
        strlen(date) >= sizeof(checked_date) || strlen(location) >= sizeof(out->location) ||
        strlen(notes) >= sizeof(out->notes)) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid event arguments");
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (apl_reference_label_normalize(name, "event name", checked_name, sizeof(checked_name),
                                      error) != APL_OK ||
        apl_reference_date_normalize(date, "event date", checked_date, sizeof(checked_date),
                                     error) != APL_OK ||
        checked_date[0] == '\0' || strcmp(date, checked_date) != 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "event name and valid ISO date are required");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *db = apl_db_handle(connection);
    if (!db) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    const char *sql =
        id == 0
            ? "INSERT INTO events(team_id,season_id,name,event_date,location,notes,created_at,updated_at) "
              "VALUES(?1,?3,?4,?5,?6,?7,strftime('%Y-%m-%dT%H:%M:%fZ','now'),"
              "strftime('%Y-%m-%dT%H:%M:%fZ','now')) RETURNING " EVENT_COLUMNS
            : "UPDATE events SET season_id=?3,name=?4,event_date=?5,location=?6,notes=?7,"
              "updated_at=strftime('%Y-%m-%dT%H:%M:%fZ','now') "
              "WHERE team_id=?1 AND id=?2 RETURNING " EVENT_COLUMNS;
    sqlite3_stmt *s = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &s, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare event write: %s",
                      sqlite3_errmsg(db));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(s, 1, team_id);
    sqlite3_bind_int64(s, 2, id);
    if (season_id)
        sqlite3_bind_int64(s, 3, season_id);
    else
        sqlite3_bind_null(s, 3);
    sqlite3_bind_text(s, 4, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 5, date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 6, location, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 7, notes, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(s);
    apl_status status = APL_OK;
    if (rc == SQLITE_ROW)
        read_event(s, out);
    else if (rc == SQLITE_DONE) {
        status = APL_ERR_NOT_FOUND;
        apl_error_set(error, status, "event not found for this team");
    } else {
        status = (rc & 0xff) == SQLITE_CONSTRAINT ? APL_ERR_INVALID_ARGUMENT : APL_ERR_DB_QUERY;
        apl_error_set(error, status, "cannot save event: %s", sqlite3_errmsg(db));
    }
    sqlite3_finalize(s);
    return status;
}

apl_status apl_event_repository_create(const apl_db_connection *c, int64_t team, int64_t season,
                                       const char *name, const char *date, const char *location,
                                       const char *notes, apl_event *out, apl_error *error)
{
    return write_event(c, team, 0, season, name, date, location, notes, out, error);
}

apl_status apl_event_repository_update(const apl_db_connection *c, int64_t team, int64_t id,
                                       int64_t season, const char *name, const char *date,
                                       const char *location, const char *notes, apl_event *out,
                                       apl_error *error)
{
    if (id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid event id");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return write_event(c, team, id, season, name, date, location, notes, out, error);
}

apl_status apl_event_repository_list(const apl_db_connection *c, int64_t team, apl_event **out,
                                     size_t *count, apl_error *error)
{
    if (!c || !out || !count || team <= 0 || !apl_db_handle(c)) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid event list arguments");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out = nullptr;
    *count = 0;
    sqlite3 *db = apl_db_handle(c);
    sqlite3_stmt *s = nullptr;
    if (sqlite3_prepare_v2(db,
                           "SELECT " EVENT_COLUMNS " FROM events WHERE team_id=?1 "
                           "ORDER BY event_date,id",
                           -1, &s, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare event list: %s", sqlite3_errmsg(db));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(s, 1, team);
    size_t capacity = 0;
    apl_event *rows = nullptr;
    int rc;
    while ((rc = sqlite3_step(s)) == SQLITE_ROW) {
        if (*count == capacity) {
            apl_event *grown = apl_grow_array(rows, &capacity, 8, sizeof(*rows));
            if (!grown) {
                free(rows);
                sqlite3_finalize(s);
                *count = 0;
                apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate event list");
                return APL_ERR_OUT_OF_MEMORY;
            }
            rows = grown;
        }
        read_event(s, &rows[(*count)++]);
    }
    if (rc != SQLITE_DONE) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read event list: %s", sqlite3_errmsg(db));
        free(rows);
        *count = 0;
        sqlite3_finalize(s);
        return APL_ERR_DB_QUERY;
    }
    sqlite3_finalize(s);
    *out = rows;
    return APL_OK;
}

apl_status apl_event_repository_get(const apl_db_connection *c, int64_t team, int64_t id,
                                    apl_event *out, apl_error *error)
{
    if (!c || !out || team <= 0 || id <= 0 || !apl_db_handle(c)) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid event lookup arguments");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *db = apl_db_handle(c);
    sqlite3_stmt *s = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT " EVENT_COLUMNS " FROM events WHERE team_id=?1 AND id=?2",
                           -1, &s, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare event lookup: %s",
                      sqlite3_errmsg(db));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(s, 1, team);
    sqlite3_bind_int64(s, 2, id);
    int rc = sqlite3_step(s);
    apl_status status = APL_OK;
    if (rc == SQLITE_ROW)
        read_event(s, out);
    else {
        status = rc == SQLITE_DONE ? APL_ERR_NOT_FOUND : APL_ERR_DB_QUERY;
        apl_error_set(error, status, "event lookup failed: %s", sqlite3_errmsg(db));
    }
    sqlite3_finalize(s);
    return status;
}

void apl_event_repository_free_list(apl_event *events)
{
    free(events);
}
