#include "db/lifecycle_repository.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/util.h"
#include "db/sqlite_util.h"

#define EVENT_COLUMNS                                                                                  \
    "e.id, e.team_id, e.action, e.event_date, e.unit_id, e.old_unit_id, e.new_unit_id, e.car_id, "      \
    "e.position, e.event_label, e.reason, e.usage, e.notes, e.created_at"

#define EVENT_COLUMNS_UNALIASED                                                                         \
    "id, team_id, action, event_date, unit_id, old_unit_id, new_unit_id, car_id, position, "            \
    "event_label, reason, usage, notes, created_at"

static void read_event(sqlite3_stmt *const statement, apl_lifecycle_event *const event)
{
    *event = (apl_lifecycle_event){};
    event->id = sqlite3_column_int64(statement, 0);
    event->team_id = sqlite3_column_int64(statement, 1);
    event->action = apl_lifecycle_action_from_code((const char *)sqlite3_column_text(statement, 2));
    apl_db_copy_column_text(statement, 3, event->event_date, sizeof(event->event_date));
    event->unit_id =
        sqlite3_column_type(statement, 4) == SQLITE_NULL ? 0 : sqlite3_column_int64(statement, 4);
    event->old_unit_id =
        sqlite3_column_type(statement, 5) == SQLITE_NULL ? 0 : sqlite3_column_int64(statement, 5);
    event->new_unit_id =
        sqlite3_column_type(statement, 6) == SQLITE_NULL ? 0 : sqlite3_column_int64(statement, 6);
    event->car_id =
        sqlite3_column_type(statement, 7) == SQLITE_NULL ? 0 : sqlite3_column_int64(statement, 7);
    apl_db_copy_column_text(statement, 8, event->position, sizeof(event->position));
    apl_db_copy_column_text(statement, 9, event->event_label, sizeof(event->event_label));
    apl_db_copy_column_text(statement, 10, event->reason, sizeof(event->reason));
    apl_db_copy_column_text(statement, 11, event->usage, sizeof(event->usage));
    apl_db_copy_column_text(statement, 12, event->notes, sizeof(event->notes));
    apl_db_copy_column_text(statement, 13, event->created_at, sizeof(event->created_at));
}

static apl_status prepare(sqlite3 *const handle, const char *const sql, sqlite3_stmt **const out,
                          apl_error *const error)
{
    if (sqlite3_prepare_v2(handle, sql, -1, out, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare lifecycle operation: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    return APL_OK;
}

static apl_status write_error(sqlite3 *const handle, apl_error *const error)
{
    const int extended = sqlite3_extended_errcode(handle);
    if (extended == SQLITE_CONSTRAINT_FOREIGNKEY) {
        apl_error_set(error, APL_ERR_NOT_FOUND, "the referenced unit/car or team no longer exists");
        return APL_ERR_NOT_FOUND;
    }
    if (extended == SQLITE_CONSTRAINT_CHECK || extended == SQLITE_CONSTRAINT_TRIGGER) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "lifecycle event violates a stored rule: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_error_set(error, APL_ERR_DB_QUERY, "lifecycle database operation failed: %s",
                  sqlite3_errmsg(handle));
    return APL_ERR_DB_QUERY;
}

static void bind_optional_id(sqlite3_stmt *const statement, const int index, const int64_t id)
{
    if (id > 0) {
        (void)sqlite3_bind_int64(statement, index, id);
    } else {
        (void)sqlite3_bind_null(statement, index);
    }
}

static apl_status append_event_sql(sqlite3 *const handle, const int64_t team_id,
                                   const apl_lifecycle_event *const event,
                                   apl_lifecycle_event *const out_event, apl_error *const error)
{
    sqlite3_stmt *statement = nullptr;
    apl_status status =
        prepare(handle,
                "INSERT INTO lifecycle_events (team_id, action, event_date, unit_id, old_unit_id, "
                "new_unit_id, car_id, position, event_label, reason, usage, notes, created_at) "
                "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, "
                "strftime('%Y-%m-%dT%H:%M:%fZ', 'now')) "
                "RETURNING " EVENT_COLUMNS_UNALIASED ";",
                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    int bind = sqlite3_bind_int64(statement, 1, team_id);
    if (bind == SQLITE_OK) {
        bind = sqlite3_bind_text(statement, 2, apl_lifecycle_action_code(event->action), -1,
                                 SQLITE_TRANSIENT);
    }
    if (bind == SQLITE_OK) {
        bind = sqlite3_bind_text(statement, 3, event->event_date, -1, SQLITE_TRANSIENT);
    }
    if (bind == SQLITE_OK && event->action != APL_LIFECYCLE_REPLACE) {
        bind = sqlite3_bind_int64(statement, 4, event->unit_id);
    } else if (bind == SQLITE_OK) {
        bind = sqlite3_bind_null(statement, 4);
    }
    if (bind == SQLITE_OK) {
        bind_optional_id(statement, 5, event->old_unit_id);
    }
    if (bind == SQLITE_OK) {
        bind_optional_id(statement, 6, event->new_unit_id);
    }
    if (bind == SQLITE_OK) {
        bind_optional_id(statement, 7, event->car_id);
    }
    if (bind == SQLITE_OK) {
        bind = sqlite3_bind_text(statement, 8, event->position, -1, SQLITE_TRANSIENT);
    }
    if (bind == SQLITE_OK) {
        bind = sqlite3_bind_text(statement, 9, event->event_label, -1, SQLITE_TRANSIENT);
    }
    if (bind == SQLITE_OK) {
        bind = sqlite3_bind_text(statement, 10, event->reason, -1, SQLITE_TRANSIENT);
    }
    if (bind == SQLITE_OK) {
        bind = sqlite3_bind_text(statement, 11, event->usage, -1, SQLITE_TRANSIENT);
    }
    if (bind == SQLITE_OK) {
        bind = sqlite3_bind_text(statement, 12, event->notes, -1, SQLITE_TRANSIENT);
    }
    if (bind != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind lifecycle event: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    const int step_status = sqlite3_step(statement);
    if (step_status != SQLITE_ROW) {
        status = write_error(handle, error);
        sqlite3_finalize(statement);
        return status;
    }
    read_event(statement, out_event);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_lifecycle_repository_append(const apl_db_connection *const connection,
                                           const int64_t team_id,
                                           const apl_lifecycle_event *const event,
                                           apl_lifecycle_event *const out_event,
                                           apl_error *const error)
{
    if (connection == nullptr || event == nullptr || out_event == nullptr || team_id <= 0 ||
        event->action == APL_LIFECYCLE_NONE || event->event_date[0] == '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "lifecycle event arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_event = (apl_lifecycle_event){};
    return append_event_sql(handle, team_id, event, out_event, error);
}

apl_status apl_lifecycle_repository_get(const apl_db_connection *const connection,
                                        const int64_t team_id, const int64_t id,
                                        apl_lifecycle_event *const out_event,
                                        apl_error *const error)
{
    if (connection == nullptr || out_event == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "lifecycle get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_event = (apl_lifecycle_event){};
    sqlite3_stmt *statement = nullptr;
    apl_status status = prepare(handle,
                                "SELECT " EVENT_COLUMNS " FROM lifecycle_events e "
                                "WHERE e.id = ?1 AND e.team_id = ?2;",
                                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    sqlite3_bind_int64(statement, 1, id);
    sqlite3_bind_int64(statement, 2, team_id);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "lifecycle event %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read lifecycle event: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_event(statement, out_event);
    sqlite3_finalize(statement);
    return APL_OK;
}

static apl_status run_event_list(sqlite3 *const handle, const apl_lifecycle_filter *const filter,
                                 apl_lifecycle_event **const out_events, size_t *const out_count,
                                 apl_error *const error)
{
    char sql[2048];
    (void)snprintf(sql, sizeof(sql),
                   "SELECT " EVENT_COLUMNS " FROM lifecycle_events e WHERE e.team_id = ?1 "
                   "AND (?2 = 0 OR e.unit_id = ?2 OR e.old_unit_id = ?2 OR e.new_unit_id = ?2) "
                   "AND (?3 = 0 OR e.car_id = ?3) "
                   "AND (?4 = '' OR e.action = ?4) "
                   "AND (?5 = '' OR e.event_date >= ?5) "
                   "AND (?6 = '' OR e.event_date <= ?6) "
                   "ORDER BY e.event_date ASC, e.id ASC;");
    sqlite3_stmt *statement = nullptr;
    apl_status status = prepare(handle, sql, &statement, error);
    if (status != APL_OK) {
        return status;
    }
    int bind = sqlite3_bind_int64(statement, 1, filter->team_id);
    if (bind == SQLITE_OK) {
        bind = sqlite3_bind_int64(statement, 2, filter->unit_id);
    }
    if (bind == SQLITE_OK) {
        bind = sqlite3_bind_int64(statement, 3, filter->car_id);
    }
    if (bind == SQLITE_OK) {
        bind = sqlite3_bind_text(statement, 4, apl_lifecycle_action_code(filter->action), -1,
                                 SQLITE_TRANSIENT);
    }
    if (bind == SQLITE_OK) {
        bind = sqlite3_bind_text(statement, 5, filter->from_date, -1, SQLITE_TRANSIENT);
    }
    if (bind == SQLITE_OK) {
        bind = sqlite3_bind_text(statement, 6, filter->to_date, -1, SQLITE_TRANSIENT);
    }
    if (bind != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind lifecycle filters: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }

    apl_lifecycle_event *events = nullptr;
    size_t count = 0;
    size_t capacity = 0;
    for (;;) {
        const int step_status = sqlite3_step(statement);
        if (step_status == SQLITE_DONE) {
            break;
        }
        if (step_status != SQLITE_ROW) {
            apl_error_set(error, APL_ERR_DB_QUERY, "cannot read lifecycle list: %s",
                          sqlite3_errmsg(handle));
            status = APL_ERR_DB_QUERY;
            break;
        }
        if (count == capacity) {
            typeof(events) const grown = apl_grow_array(events, &capacity, 8, sizeof(*events));
            if (grown == nullptr) {
                apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate lifecycle list");
                status = APL_ERR_OUT_OF_MEMORY;
                break;
            }
            events = grown;
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

apl_status apl_lifecycle_repository_list(const apl_db_connection *const connection,
                                         const apl_lifecycle_filter *const filter,
                                         apl_lifecycle_event **const out_events,
                                         size_t *const out_count, apl_error *const error)
{
    if (connection == nullptr || out_events == nullptr || out_count == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "lifecycle list arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_events = nullptr;
    *out_count = 0;
    if (filter == nullptr || filter->team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "lifecycle list requires a team scope");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return run_event_list(handle, filter, out_events, out_count, error);
}

apl_status apl_lifecycle_repository_list_unit(const apl_db_connection *const connection,
                                              const int64_t team_id, const int64_t unit_id,
                                              apl_lifecycle_history_entry **const out_entries,
                                              size_t *const out_count, apl_error *const error)
{
    if (connection == nullptr || out_entries == nullptr || out_count == nullptr || team_id <= 0 ||
        unit_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "lifecycle unit list arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_entries = nullptr;
    *out_count = 0;
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_lifecycle_event *events = nullptr;
    size_t count = 0;
    apl_lifecycle_filter filter = {.team_id = team_id, .unit_id = unit_id};
    apl_status status = run_event_list(handle, &filter, &events, &count, error);
    if (status != APL_OK) {
        return status;
    }
    apl_lifecycle_history_entry *entries = nullptr;
    size_t entry_capacity = 0;
    if (count > 0) {
        entries = calloc(count, sizeof(*entries));
        if (entries == nullptr) {
            free(events);
            apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate lifecycle history");
            return APL_ERR_OUT_OF_MEMORY;
        }
        entry_capacity = count;
    }
    for (size_t index = 0; index < count; ++index) {
        entries[index].event = events[index];
        entries[index].current = false;
    }
    free(events);
    *out_entries = entries;
    *out_count = count;
    (void)entry_capacity;
    return APL_OK;
}

typedef struct fitment_slot {
    int64_t car_id;
    char position[APL_LIFECYCLE_POSITION_MAX];
    int64_t unit_id;
    int64_t type_id;
} fitment_slot;

static fitment_slot *find_slot(fitment_slot *const slots, size_t count, const int64_t car_id,
                               const char *const position)
{
    for (size_t index = 0; index < count; ++index) {
        if (slots[index].car_id == car_id && strcmp(slots[index].position, position) == 0) {
            return &slots[index];
        }
    }
    return nullptr;
}

static int64_t unit_type(sqlite3 *const handle, const int64_t unit_id)
{
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle, "SELECT type_id FROM part_units WHERE id = ?1;", -1, &statement,
                           nullptr) != SQLITE_OK) {
        return 0;
    }
    sqlite3_bind_int64(statement, 1, unit_id);
    int64_t type_id = 0;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        type_id = sqlite3_column_int64(statement, 0);
    }
    sqlite3_finalize(statement);
    return type_id;
}

apl_status apl_lifecycle_repository_current_fitment(const apl_db_connection *const connection,
                                                    const int64_t team_id, apl_fitment_row **const
                                                        out_rows,
                                                    size_t *const out_count, apl_error *const error)
{
    if (connection == nullptr || out_rows == nullptr || out_count == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "current fitment arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_rows = nullptr;
    *out_count = 0;
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_lifecycle_event *events = nullptr;
    size_t count = 0;
    apl_lifecycle_filter filter = {.team_id = team_id};
    apl_status status = run_event_list(handle, &filter, &events, &count, error);
    if (status != APL_OK) {
        return status;
    }

    fitment_slot *slots = nullptr;
    size_t slot_count = 0;
    size_t slot_capacity = 0;
    for (size_t index = 0; index < count; ++index) {
        const apl_lifecycle_event *const event = &events[index];
        if (event->action == APL_LIFECYCLE_REPAIR) {
            continue; /* repair does not change fitment */
        }
        if (event->action == APL_LIFECYCLE_INSTALL) {
            fitment_slot *slot = find_slot(slots, slot_count, event->car_id, event->position);
            if (slot == nullptr) {
                if (slot_count == slot_capacity) {
                    typeof(slots) const grown =
                        apl_grow_array(slots, &slot_capacity, 4, sizeof(*slots));
                    if (grown == nullptr) {
                        free(slots);
                        free(events);
                        apl_error_set(error, APL_ERR_OUT_OF_MEMORY,
                                      "cannot allocate current fitment slots");
                        return APL_ERR_OUT_OF_MEMORY;
                    }
                    slots = grown;
                }
                slot = &slots[slot_count++];
                slot->car_id = event->car_id;
                (void)snprintf(slot->position, sizeof(slot->position), "%s", event->position);
                slot->unit_id = 0;
                slot->type_id = 0;
            }
            slot->unit_id = event->unit_id;
            slot->type_id = unit_type(handle, event->unit_id);
        } else if (event->action == APL_LIFECYCLE_REPLACE) {
            fitment_slot *slot = find_slot(slots, slot_count, event->car_id, event->position);
            if (slot != nullptr && slot->unit_id == event->old_unit_id) {
                slot->unit_id = event->new_unit_id;
                slot->type_id = unit_type(handle, event->new_unit_id);
            }
        } else { /* remove or dispose */
            fitment_slot *slot = find_slot(slots, slot_count, event->car_id, event->position);
            if (slot != nullptr && slot->unit_id == event->unit_id) {
                slot->unit_id = 0;
                slot->type_id = 0;
            }
        }
    }
    free(events);

    apl_fitment_row *rows = nullptr;
    size_t row_count = 0;
    size_t row_capacity = 0;
    for (size_t index = 0; index < slot_count; ++index) {
        if (slots[index].unit_id == 0) {
            continue;
        }
        if (row_count == row_capacity) {
            typeof(rows) const grown = apl_grow_array(rows, &row_capacity, 4, sizeof(*rows));
            if (grown == nullptr) {
                free(slots);
                apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate current fitment list");
                return APL_ERR_OUT_OF_MEMORY;
            }
            rows = grown;
        }
        rows[row_count].unit_id = slots[index].unit_id;
        rows[row_count].type_id = slots[index].type_id;
        rows[row_count].car_id = slots[index].car_id;
        (void)snprintf(rows[row_count].position, sizeof(rows[row_count].position), "%s",
                       slots[index].position);
        ++row_count;
    }
    free(slots);
    *out_rows = rows;
    *out_count = row_count;
    return APL_OK;
}

void apl_lifecycle_repository_free_events(apl_lifecycle_event *const events)
{
    free(events);
}

void apl_lifecycle_repository_free_history(apl_lifecycle_history_entry *const entries)
{
    free(entries);
}

void apl_lifecycle_repository_free_fitment(apl_fitment_row *const rows)
{
    free(rows);
}