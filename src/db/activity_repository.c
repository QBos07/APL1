#include "db/activity_repository.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>

#define EVENT_COLUMNS "id, team_id, occurred_at, action, entity_type, entity_id, summary"

static void copy_text(sqlite3_stmt *const statement, const int column, char *const out,
                      const size_t out_size)
{
    const unsigned char *const text = sqlite3_column_text(statement, column);
    (void)snprintf(out, out_size, "%s", (text != nullptr) ? (const char *)text : "");
}

static void read_event(sqlite3_stmt *const statement, apl_activity_event *const event)
{
    event->id = sqlite3_column_int64(statement, 0);
    event->team_id =
        sqlite3_column_type(statement, 1) == SQLITE_NULL ? 0 : sqlite3_column_int64(statement, 1);
    copy_text(statement, 2, event->occurred_at, sizeof(event->occurred_at));
    copy_text(statement, 3, event->action, sizeof(event->action));
    copy_text(statement, 4, event->entity_type, sizeof(event->entity_type));
    event->entity_id =
        sqlite3_column_type(statement, 5) == SQLITE_NULL ? 0 : sqlite3_column_int64(statement, 5);
    copy_text(statement, 6, event->summary, sizeof(event->summary));
}

static apl_status prepare(sqlite3 *const handle, const char *const sql, sqlite3_stmt **const out,
                          apl_error *const error)
{
    if (sqlite3_prepare_v2(handle, sql, -1, out, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare activity operation: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    return APL_OK;
}

apl_status apl_activity_repository_append(const apl_db_connection *const connection,
                                          const apl_activity_event *const event,
                                          const apl_activity_change *const changes,
                                          const size_t change_count,
                                          apl_activity_event *const out_event,
                                          apl_error *const error)
{
    if (connection == nullptr || event == nullptr || (changes == nullptr && change_count > 0) ||
        event->action[0] == '\0' || event->entity_type[0] == '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "activity append arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    apl_status status = prepare(handle,
                                "INSERT INTO activity_events (team_id, occurred_at, action, "
                                "entity_type, entity_id, summary) VALUES "
                                "(?1, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), ?2, ?3, ?4, ?5) "
                                "RETURNING " EVENT_COLUMNS ";",
                                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    int bind_status = SQLITE_OK;
    if (event->team_id > 0) {
        bind_status = sqlite3_bind_int64(statement, 1, event->team_id);
    } else {
        bind_status = sqlite3_bind_null(statement, 1);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 2, event->action, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 3, event->entity_type, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        if (event->entity_id > 0) {
            bind_status = sqlite3_bind_int64(statement, 4, event->entity_id);
        } else {
            bind_status = sqlite3_bind_null(statement, 4);
        }
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 5, event->summary, -1, SQLITE_TRANSIENT);
    }
    if (bind_status != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind activity event: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }

    apl_activity_event stored = {};
    const int step_status = sqlite3_step(statement);
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot append activity event: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_event(statement, &stored);
    sqlite3_finalize(statement);

    if (change_count > 0) {
        status = prepare(handle,
                         "INSERT INTO activity_changes (event_id, field_key, old_value, new_value) "
                         "VALUES (?1, ?2, ?3, ?4);",
                         &statement, error);
        if (status != APL_OK) {
            return status;
        }
        for (size_t index = 0; index < change_count; ++index) {
            const apl_activity_change *const change = &changes[index];
            if (change->field[0] == '\0') {
                apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                              "activity change has an empty field name");
                sqlite3_finalize(statement);
                return APL_ERR_INVALID_ARGUMENT;
            }
            (void)sqlite3_reset(statement);
            (void)sqlite3_clear_bindings(statement);
            bind_status = sqlite3_bind_int64(statement, 1, stored.id);
            if (bind_status == SQLITE_OK) {
                bind_status = sqlite3_bind_text(statement, 2, change->field, -1, SQLITE_TRANSIENT);
            }
            if (bind_status == SQLITE_OK) {
                if (change->old_missing) {
                    bind_status = sqlite3_bind_null(statement, 3);
                } else {
                    bind_status =
                        sqlite3_bind_text(statement, 3, change->old_value, -1, SQLITE_TRANSIENT);
                }
            }
            if (bind_status == SQLITE_OK) {
                if (change->new_missing) {
                    bind_status = sqlite3_bind_null(statement, 4);
                } else {
                    bind_status =
                        sqlite3_bind_text(statement, 4, change->new_value, -1, SQLITE_TRANSIENT);
                }
            }
            if (bind_status != SQLITE_OK || sqlite3_step(statement) != SQLITE_DONE) {
                apl_error_set(error, APL_ERR_DB_QUERY, "cannot append activity change: %s",
                              sqlite3_errmsg(handle));
                sqlite3_finalize(statement);
                return APL_ERR_DB_QUERY;
            }
        }
        sqlite3_finalize(statement);
    }

    if (out_event != nullptr) {
        *out_event = stored;
    }
    return APL_OK;
}

apl_status apl_activity_repository_get(const apl_db_connection *const connection, const int64_t id,
                                       apl_activity_event *const out_event, apl_error *const error)
{
    if (connection == nullptr || out_event == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "activity get arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_event = (apl_activity_event){};

    sqlite3_stmt *statement = nullptr;
    apl_status status = prepare(
        handle, "SELECT " EVENT_COLUMNS " FROM activity_events WHERE id = ?1;", &statement, error);
    if (status != APL_OK) {
        return status;
    }
    sqlite3_bind_int64(statement, 1, id);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "activity event %lld was not found", (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read activity event: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_event(statement, out_event);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_activity_repository_list(const apl_db_connection *const connection,
                                        const apl_activity_filter *const filter,
                                        apl_activity_event **const out_events,
                                        size_t *const out_count, apl_error *const error)
{
    if (connection == nullptr || out_events == nullptr || out_count == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "activity list arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_events = nullptr;
    *out_count = 0;
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    const apl_activity_filter empty_filter = {};
    const apl_activity_filter *const active_filter = (filter != nullptr) ? filter : &empty_filter;

    sqlite3_stmt *statement = nullptr;
    apl_status status = prepare(handle,
                                "SELECT " EVENT_COLUMNS " FROM activity_events "
                                "WHERE (?1 = 0 OR team_id = ?1 OR (?6 = 1 AND team_id IS NULL)) "
                                "AND (?2 = '' OR action = ?2) "
                                "AND (?3 = '' OR entity_type = ?3) "
                                "AND (?4 = '' OR substr(occurred_at, 1, 10) >= ?4) "
                                "AND (?5 = '' OR substr(occurred_at, 1, 10) <= ?5) "
                                "ORDER BY occurred_at DESC, id DESC;",
                                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    int bind_status = sqlite3_bind_int64(statement, 1, active_filter->team_id);
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 2, active_filter->action, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status =
            sqlite3_bind_text(statement, 3, active_filter->entity_type, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status =
            sqlite3_bind_text(statement, 4, active_filter->from_date, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 5, active_filter->to_date, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_int(statement, 6, active_filter->include_teamless ? 1 : 0);
    }
    if (bind_status != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind activity filters: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }

    apl_activity_event *events = nullptr;
    size_t count = 0;
    size_t capacity = 0;
    for (;;) {
        const int step_status = sqlite3_step(statement);
        if (step_status == SQLITE_DONE) {
            break;
        }
        if (step_status != SQLITE_ROW) {
            apl_error_set(error, APL_ERR_DB_QUERY, "cannot read activity list: %s",
                          sqlite3_errmsg(handle));
            status = APL_ERR_DB_QUERY;
            break;
        }
        if (count == capacity) {
            const size_t grown_capacity = (capacity == 0) ? 16 : capacity * 2;
            apl_activity_event *const grown = realloc(events, grown_capacity * sizeof(*events));
            if (grown == nullptr) {
                apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate activity list");
                status = APL_ERR_OUT_OF_MEMORY;
                break;
            }
            events = grown;
            capacity = grown_capacity;
        }
        read_event(statement, &events[count]);
        ++count;
    }
    sqlite3_finalize(statement);
    if (status != APL_OK) {
        free(events);
        return status;
    }
    *out_events = events;
    *out_count = count;
    return APL_OK;
}

apl_status apl_activity_repository_list_changes(const apl_db_connection *const connection,
                                                const int64_t event_id,
                                                apl_activity_change **const out_changes,
                                                size_t *const out_count, apl_error *const error)
{
    if (connection == nullptr || out_changes == nullptr || out_count == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "activity change list arguments are null");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_changes = nullptr;
    *out_count = 0;
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    apl_status status = prepare(handle,
                                "SELECT field_key, old_value, new_value FROM activity_changes "
                                "WHERE event_id = ?1 ORDER BY id ASC;",
                                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    sqlite3_bind_int64(statement, 1, event_id);

    apl_activity_change *changes = nullptr;
    size_t count = 0;
    size_t capacity = 0;
    for (;;) {
        const int step_status = sqlite3_step(statement);
        if (step_status == SQLITE_DONE) {
            break;
        }
        if (step_status != SQLITE_ROW) {
            apl_error_set(error, APL_ERR_DB_QUERY, "cannot read activity changes: %s",
                          sqlite3_errmsg(handle));
            status = APL_ERR_DB_QUERY;
            break;
        }
        if (count == capacity) {
            const size_t grown_capacity = (capacity == 0) ? 8 : capacity * 2;
            apl_activity_change *const grown = realloc(changes, grown_capacity * sizeof(*changes));
            if (grown == nullptr) {
                apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate activity changes");
                status = APL_ERR_OUT_OF_MEMORY;
                break;
            }
            changes = grown;
            capacity = grown_capacity;
        }
        apl_activity_change *const change = &changes[count];
        *change = (apl_activity_change){};
        copy_text(statement, 0, change->field, sizeof(change->field));
        change->old_missing = sqlite3_column_type(statement, 1) == SQLITE_NULL;
        if (!change->old_missing) {
            copy_text(statement, 1, change->old_value, sizeof(change->old_value));
        }
        change->new_missing = sqlite3_column_type(statement, 2) == SQLITE_NULL;
        if (!change->new_missing) {
            copy_text(statement, 2, change->new_value, sizeof(change->new_value));
        }
        ++count;
    }
    sqlite3_finalize(statement);
    if (status != APL_OK) {
        free(changes);
        return status;
    }
    *out_changes = changes;
    *out_count = count;
    return APL_OK;
}

void apl_activity_repository_free_events(apl_activity_event *const events)
{
    free(events);
}

void apl_activity_repository_free_changes(apl_activity_change *const changes)
{
    free(changes);
}
