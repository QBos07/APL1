#include "db/part_type_repository.h"

#include <sqlite3.h>
#include <stdlib.h>

#include "core/util.h"
#include "db/sqlite_util.h"

#define PART_TYPE_COLUMNS                                                                             \
    "id, team_id, category, category_key, name, name_key, description, notes, archived, created_at, " \
    "updated_at, archived_at"

static void read_type(sqlite3_stmt *const statement, apl_part_type *const type)
{
    *type = (apl_part_type){};
    type->id = sqlite3_column_int64(statement, 0);
    type->team_id = sqlite3_column_int64(statement, 1);
    apl_db_copy_column_text(statement, 2, type->category, sizeof(type->category));
    apl_db_copy_column_text(statement, 3, type->category_key, sizeof(type->category_key));
    apl_db_copy_column_text(statement, 4, type->name, sizeof(type->name));
    apl_db_copy_column_text(statement, 5, type->name_key, sizeof(type->name_key));
    apl_db_copy_column_text(statement, 6, type->description, sizeof(type->description));
    apl_db_copy_column_text(statement, 7, type->notes, sizeof(type->notes));
    type->meta.archived = sqlite3_column_int(statement, 8) != 0;
    apl_db_copy_column_text(statement, 9, type->meta.created_at, sizeof(type->meta.created_at));
    apl_db_copy_column_text(statement, 10, type->meta.updated_at, sizeof(type->meta.updated_at));
    apl_db_copy_column_text(statement, 11, type->meta.archived_at, sizeof(type->meta.archived_at));
}

static apl_status prepare(sqlite3 *const handle, const char *const sql, sqlite3_stmt **const out,
                          apl_error *const error)
{
    if (sqlite3_prepare_v2(handle, sql, -1, out, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare part type operation: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    return APL_OK;
}

static apl_status write_error(sqlite3 *const handle, apl_error *const error)
{
    const int extended = sqlite3_extended_errcode(handle);
    if (extended == SQLITE_CONSTRAINT_UNIQUE || extended == SQLITE_CONSTRAINT_PRIMARYKEY) {
        apl_error_set(error, APL_ERR_CONFLICT,
                      "a part type with this category and name already exists for the team");
        return APL_ERR_CONFLICT;
    }
    if (extended == SQLITE_CONSTRAINT_FOREIGNKEY) {
        apl_error_set(error, APL_ERR_NOT_FOUND, "the owning team no longer exists");
        return APL_ERR_NOT_FOUND;
    }
    if (extended == SQLITE_CONSTRAINT_CHECK || extended == SQLITE_CONSTRAINT_TRIGGER) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part type data violates a stored rule: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_error_set(error, APL_ERR_DB_QUERY, "part type database operation failed: %s",
                  sqlite3_errmsg(handle));
    return APL_ERR_DB_QUERY;
}

static int bind_type_fields(sqlite3_stmt *const statement, const int first_index,
                            const apl_part_type *const type)
{
    int index = first_index;
    int status = sqlite3_bind_text(statement, index++, type->category, -1, SQLITE_TRANSIENT);
    if (status == SQLITE_OK) {
        status = sqlite3_bind_text(statement, index++, type->category_key, -1, SQLITE_TRANSIENT);
    }
    if (status == SQLITE_OK) {
        status = sqlite3_bind_text(statement, index++, type->name, -1, SQLITE_TRANSIENT);
    }
    if (status == SQLITE_OK) {
        status = sqlite3_bind_text(statement, index++, type->name_key, -1, SQLITE_TRANSIENT);
    }
    if (status == SQLITE_OK) {
        status = sqlite3_bind_text(statement, index++, type->description, -1, SQLITE_TRANSIENT);
    }
    if (status == SQLITE_OK) {
        status = sqlite3_bind_text(statement, index, type->notes, -1, SQLITE_TRANSIENT);
    }
    return status;
}

static apl_status write_and_read(sqlite3 *const handle, const char *const sql,
                                 const int64_t team_id, const int64_t id, const bool bind_id,
                                 const apl_part_type *const type, apl_part_type *const out_type,
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
        bind_status = bind_type_fields(statement, fields_index, type);
    }
    if (bind_status != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind part type operation: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }

    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE && bind_id) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "part type %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        status = write_error(handle, error);
        sqlite3_finalize(statement);
        return status;
    }
    read_type(statement, out_type);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_part_type_repository_create(const apl_db_connection *const connection,
                                           const apl_part_type *const type,
                                           apl_part_type *const out_type, apl_error *const error)
{
    if (connection == nullptr || type == nullptr || out_type == nullptr || type->team_id <= 0 ||
        type->category[0] == '\0' || type->name[0] == '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part type create arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_type = (apl_part_type){};
    return write_and_read(
        handle,
        "INSERT INTO part_types (team_id, category, category_key, name, name_key, description, "
        "notes, archived, created_at, updated_at) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, 0, "
        "strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), strftime('%Y-%m-%dT%H:%M:%fZ', 'now')) "
        "RETURNING " PART_TYPE_COLUMNS ";",
        type->team_id, 0, false, type, out_type, error);
}

apl_status apl_part_type_repository_get(const apl_db_connection *const connection,
                                        const int64_t team_id, const int64_t id,
                                        apl_part_type *const out_type, apl_error *const error)
{
    if (connection == nullptr || out_type == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part type get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_type = (apl_part_type){};

    sqlite3_stmt *statement = nullptr;
    apl_status status = prepare(
        handle, "SELECT " PART_TYPE_COLUMNS " FROM part_types WHERE id = ?1 AND team_id = ?2;",
        &statement, error);
    if (status != APL_OK) {
        return status;
    }
    sqlite3_bind_int64(statement, 1, id);
    sqlite3_bind_int64(statement, 2, team_id);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "part type %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read part type: %s", sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_type(statement, out_type);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_part_type_repository_update(const apl_db_connection *const connection,
                                           const apl_part_type *const type,
                                           apl_part_type *const out_type, apl_error *const error)
{
    if (connection == nullptr || type == nullptr || out_type == nullptr || type->team_id <= 0 ||
        type->id <= 0 || type->category[0] == '\0' || type->name[0] == '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part type update arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_type = (apl_part_type){};
    return write_and_read(
        handle,
        "UPDATE part_types SET category = ?3, category_key = ?4, name = ?5, name_key = ?6, "
        "description = ?7, notes = ?8, updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now') "
        "WHERE id = ?2 AND team_id = ?1 RETURNING " PART_TYPE_COLUMNS ";",
        type->team_id, type->id, true, type, out_type, error);
}

apl_status apl_part_type_repository_archive(const apl_db_connection *const connection,
                                            const int64_t team_id, const int64_t id,
                                            apl_part_type *const out_type, apl_error *const error)
{
    if (connection == nullptr || out_type == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part type archive arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_type = (apl_part_type){};

    sqlite3_stmt *statement = nullptr;
    apl_status status =
        prepare(handle,
                "UPDATE part_types SET archived = 1, "
                "archived_at = COALESCE(archived_at, strftime('%Y-%m-%dT%H:%M:%fZ', 'now')), "
                "updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now') "
                "WHERE id = ?2 AND team_id = ?1 RETURNING " PART_TYPE_COLUMNS ";",
                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    sqlite3_bind_int64(statement, 1, team_id);
    sqlite3_bind_int64(statement, 2, id);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "part type %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot archive part type: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_type(statement, out_type);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_part_type_repository_list(const apl_db_connection *const connection,
                                         const apl_part_type_filter *const filter,
                                         apl_part_type **const out_types, size_t *const out_count,
                                         apl_error *const error)
{
    if (connection == nullptr || out_types == nullptr || out_count == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part type list arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_types = nullptr;
    *out_count = 0;
    if (filter == nullptr || filter->team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part type list requires a team scope");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    apl_status status = prepare(handle,
                                "SELECT " PART_TYPE_COLUMNS " FROM part_types WHERE team_id = ?1 "
                                "AND (?2 = 1 OR archived = 0) "
                                "AND (?3 = '' OR category_key = ?3) "
                                "ORDER BY archived ASC, category COLLATE NOCASE ASC, "
                                "name COLLATE NOCASE ASC, id ASC;",
                                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    int bind_status = sqlite3_bind_int64(statement, 1, filter->team_id);
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_int(statement, 2, filter->include_archived ? 1 : 0);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 3, filter->category_key, -1, SQLITE_TRANSIENT);
    }
    if (bind_status != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind part type filters: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }

    apl_part_type *types = nullptr;
    size_t count = 0;
    size_t capacity = 0;
    for (;;) {
        const int step_status = sqlite3_step(statement);
        if (step_status == SQLITE_DONE) {
            break;
        }
        if (step_status != SQLITE_ROW) {
            apl_error_set(error, APL_ERR_DB_QUERY, "cannot read part type list: %s",
                          sqlite3_errmsg(handle));
            status = APL_ERR_DB_QUERY;
            break;
        }
        if (count == capacity) {
            typeof(types) const grown = apl_grow_array(types, &capacity, 8, sizeof(*types));
            if (grown == nullptr) {
                apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate part type list");
                status = APL_ERR_OUT_OF_MEMORY;
                break;
            }
            types = grown;
        }
        read_type(statement, &types[count]);
        ++count;
    }
    sqlite3_finalize(statement);
    if (status != APL_OK) {
        free(types);
        return status;
    }
    *out_types = types;
    *out_count = count;
    return APL_OK;
}

void apl_part_type_repository_free_list(apl_part_type *const types)
{
    free(types);
}