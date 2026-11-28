#include "db/part_unit_repository.h"

#include <sqlite3.h>
#include <stdlib.h>

#include "core/util.h"
#include "db/sqlite_util.h"

#define UNIT_TEAM "(SELECT team_id FROM part_types WHERE part_types.id=part_units.type_id)"
#define UNIT_FIELDS                                                                                \
    "type_id, serial_number, part_number, condition_label, condition_key, notes, "                 \
    "archived, created_at, updated_at, archived_at"
#define PART_UNIT_COLUMNS "id, team_id, " UNIT_FIELDS
#define PART_UNIT_RETURNING "id, " UNIT_TEAM ", " UNIT_FIELDS

static void read_unit(sqlite3_stmt *const statement, apl_part_unit *const unit)
{
    *unit = (apl_part_unit){};
    unit->id = sqlite3_column_int64(statement, 0);
    unit->team_id = sqlite3_column_int64(statement, 1);
    unit->type_id =
        sqlite3_column_type(statement, 2) == SQLITE_NULL ? 0 : sqlite3_column_int64(statement, 2);
    apl_db_copy_column_text(statement, 3, unit->serial_number, sizeof(unit->serial_number));
    apl_db_copy_column_text(statement, 4, unit->part_number, sizeof(unit->part_number));
    apl_db_copy_column_text(statement, 5, unit->condition, sizeof(unit->condition));
    apl_db_copy_column_text(statement, 6, unit->condition_key, sizeof(unit->condition_key));
    apl_db_copy_column_text(statement, 7, unit->notes, sizeof(unit->notes));
    unit->meta.archived = sqlite3_column_int(statement, 8) != 0;
    apl_db_copy_column_text(statement, 9, unit->meta.created_at, sizeof(unit->meta.created_at));
    apl_db_copy_column_text(statement, 10, unit->meta.updated_at, sizeof(unit->meta.updated_at));
    apl_db_copy_column_text(statement, 11, unit->meta.archived_at, sizeof(unit->meta.archived_at));
}

static apl_status prepare(sqlite3 *const handle, const char *const sql, sqlite3_stmt **const out,
                          apl_error *const error)
{
    if (sqlite3_prepare_v2(handle, sql, -1, out, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare part unit operation: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    return APL_OK;
}

static apl_status write_error(sqlite3 *const handle, apl_error *const error)
{
    const int extended = sqlite3_extended_errcode(handle);
    if (extended == SQLITE_CONSTRAINT_FOREIGNKEY) {
        apl_error_set(error, APL_ERR_NOT_FOUND, "the owning team or type no longer exists");
        return APL_ERR_NOT_FOUND;
    }
    if (extended == SQLITE_CONSTRAINT_CHECK || extended == SQLITE_CONSTRAINT_TRIGGER) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part unit data violates a stored rule: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_error_set(error, APL_ERR_DB_QUERY, "part unit database operation failed: %s",
                  sqlite3_errmsg(handle));
    return APL_ERR_DB_QUERY;
}

static int bind_unit_fields(sqlite3_stmt *const statement, const int first_index,
                            const apl_part_unit *const unit)
{
    int index = first_index;
    int status = sqlite3_bind_int64(statement, index++, unit->type_id);
    if (status == SQLITE_OK) {
        status = sqlite3_bind_text(statement, index++, unit->serial_number, -1, SQLITE_TRANSIENT);
    }
    if (status == SQLITE_OK) {
        status = sqlite3_bind_text(statement, index++, unit->part_number, -1, SQLITE_TRANSIENT);
    }
    if (status == SQLITE_OK) {
        status = sqlite3_bind_text(statement, index++, unit->condition, -1, SQLITE_TRANSIENT);
    }
    if (status == SQLITE_OK) {
        status = sqlite3_bind_text(statement, index++, unit->condition_key, -1, SQLITE_TRANSIENT);
    }
    if (status == SQLITE_OK) {
        status = sqlite3_bind_text(statement, index, unit->notes, -1, SQLITE_TRANSIENT);
    }
    return status;
}

static apl_status write_and_read(sqlite3 *const handle, const char *const sql,
                                 const int64_t team_id, const int64_t id, const bool bind_id,
                                 const apl_part_unit *const unit, apl_part_unit *const out_unit,
                                 apl_error *const error)
{
    sqlite3_stmt *statement = nullptr;
    apl_status status = prepare(handle, sql, &statement, error);
    if (status != APL_OK) {
        return status;
    }
    int bind_status = sqlite3_bind_int64(statement, 1, team_id);
    int next = 2;
    if (bind_id && bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_int64(statement, next++, id);
    }
    if (bind_status == SQLITE_OK) {
        const int fields_index = bind_id ? next : 2;
        bind_status = bind_unit_fields(statement, fields_index, unit);
    }
    if (bind_status != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind part unit operation: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }

    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        const apl_status missing = bind_id ? APL_ERR_NOT_FOUND : APL_ERR_INVALID_ARGUMENT;
        apl_error_set(error, missing, "part unit %lld or its type was not found for this team",
                      (long long)id);
        return missing;
    }
    if (step_status != SQLITE_ROW) {
        status = write_error(handle, error);
        sqlite3_finalize(statement);
        return status;
    }
    read_unit(statement, out_unit);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_part_unit_repository_create(const apl_db_connection *const connection,
                                           const apl_part_unit *const unit,
                                           apl_part_unit *const out_unit, apl_error *const error)
{
    if (connection == nullptr || unit == nullptr || out_unit == nullptr || unit->team_id <= 0 ||
        unit->type_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part unit create arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_unit = (apl_part_unit){};
    return write_and_read(
        handle,
        "INSERT INTO part_units (type_id, serial_number, part_number, condition_label, "
        "condition_key, notes, archived, created_at, updated_at) SELECT ?2, ?3, ?4, ?5, "
        "?6, ?7, 0, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), strftime('%Y-%m-%dT%H:%M:%fZ', 'now') "
        "WHERE EXISTS (SELECT 1 FROM part_types WHERE id=?2 AND team_id=?1) "
        "RETURNING " PART_UNIT_RETURNING ";",
        unit->team_id, 0, false, unit, out_unit, error);
}

apl_status apl_part_unit_repository_get(const apl_db_connection *const connection,
                                        const int64_t team_id, const int64_t id,
                                        apl_part_unit *const out_unit, apl_error *const error)
{
    if (connection == nullptr || out_unit == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part unit get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_unit = (apl_part_unit){};

    sqlite3_stmt *statement = nullptr;
    apl_status status = prepare(handle,
                                "SELECT " PART_UNIT_COLUMNS
                                " FROM part_units_scoped WHERE id = ?1 AND team_id = ?2;",
                                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    sqlite3_bind_int64(statement, 1, id);
    sqlite3_bind_int64(statement, 2, team_id);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "part unit %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read part unit: %s", sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_unit(statement, out_unit);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_part_unit_repository_update(const apl_db_connection *const connection,
                                           const apl_part_unit *const unit,
                                           apl_part_unit *const out_unit, apl_error *const error)
{
    if (connection == nullptr || unit == nullptr || out_unit == nullptr || unit->team_id <= 0 ||
        unit->id <= 0 || unit->type_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part unit update arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_unit = (apl_part_unit){};
    return write_and_read(
        handle,
        "UPDATE part_units SET type_id = ?3, serial_number = ?4, part_number = ?5, "
        "condition_label = ?6, condition_key = ?7, notes = ?8, "
        "updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now') "
        "WHERE id = ?2 AND " UNIT_TEAM " = ?1 "
        "AND EXISTS (SELECT 1 FROM part_types WHERE id=?3 AND team_id=?1) "
        "RETURNING " PART_UNIT_RETURNING ";",
        unit->team_id, unit->id, true, unit, out_unit, error);
}

apl_status apl_part_unit_repository_archive(const apl_db_connection *const connection,
                                            const int64_t team_id, const int64_t id,
                                            apl_part_unit *const out_unit, apl_error *const error)
{
    if (connection == nullptr || out_unit == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part unit archive arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_unit = (apl_part_unit){};

    sqlite3_stmt *statement = nullptr;
    apl_status status =
        prepare(handle,
                "UPDATE part_units SET archived = 1, "
                "archived_at = COALESCE(archived_at, strftime('%Y-%m-%dT%H:%M:%fZ', 'now')), "
                "updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now') "
                "WHERE id = ?2 AND " UNIT_TEAM " = ?1 RETURNING " PART_UNIT_RETURNING ";",
                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    sqlite3_bind_int64(statement, 1, team_id);
    sqlite3_bind_int64(statement, 2, id);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "part unit %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot archive part unit: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_unit(statement, out_unit);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_part_unit_repository_list(const apl_db_connection *const connection,
                                         const apl_part_unit_filter *const filter,
                                         apl_part_unit **const out_units, size_t *const out_count,
                                         apl_error *const error)
{
    if (connection == nullptr || out_units == nullptr || out_count == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part unit list arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_units = nullptr;
    *out_count = 0;
    if (filter == nullptr || filter->team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part unit list requires a team scope");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    apl_status status =
        prepare(handle,
                "SELECT " PART_UNIT_COLUMNS " FROM part_units_scoped WHERE team_id = ?1 "
                "AND (?2 = 1 OR archived = 0) "
                "AND (?3 = 0 OR type_id = ?3) "
                "ORDER BY archived ASC, serial_number COLLATE NOCASE ASC, "
                "part_number COLLATE NOCASE ASC, id ASC;",
                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    int bind_status = sqlite3_bind_int64(statement, 1, filter->team_id);
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_int(statement, 2, filter->include_archived ? 1 : 0);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_int64(statement, 3, filter->type_id);
    }
    if (bind_status != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind part unit filters: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }

    apl_part_unit *units = nullptr;
    size_t count = 0;
    size_t capacity = 0;
    for (;;) {
        const int step_status = sqlite3_step(statement);
        if (step_status == SQLITE_DONE) {
            break;
        }
        if (step_status != SQLITE_ROW) {
            apl_error_set(error, APL_ERR_DB_QUERY, "cannot read part unit list: %s",
                          sqlite3_errmsg(handle));
            status = APL_ERR_DB_QUERY;
            break;
        }
        if (count == capacity) {
            typeof(units) const grown = apl_grow_array(units, &capacity, 8, sizeof(*units));
            if (grown == nullptr) {
                apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate part unit list");
                status = APL_ERR_OUT_OF_MEMORY;
                break;
            }
            units = grown;
        }
        read_unit(statement, &units[count]);
        ++count;
    }
    sqlite3_finalize(statement);
    if (status != APL_OK) {
        free(units);
        return status;
    }
    *out_units = units;
    *out_count = count;
    return APL_OK;
}

void apl_part_unit_repository_free_list(apl_part_unit *const units)
{
    free(units);
}
