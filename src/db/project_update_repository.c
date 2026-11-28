#include "db/project_update_repository.h"

#include <sqlite3.h>
#include <stdlib.h>

#include "core/util.h"
#include "db/sqlite_util.h"

#define UPDATE_COLUMNS                                                                              \
    "u.id, u.project_id, u.update_date, u.summary, u.status_change, u.status_change_key, u.notes, " \
    "u.created_at"

static void read_update(sqlite3_stmt *const statement, apl_project_update *const update)
{
    *update = (apl_project_update){};
    update->id = sqlite3_column_int64(statement, 0);
    update->project_id = sqlite3_column_int64(statement, 1);
    apl_db_copy_column_text(statement, 2, update->update_date, sizeof(update->update_date));
    apl_db_copy_column_text(statement, 3, update->summary, sizeof(update->summary));
    apl_db_copy_column_text(statement, 4, update->status_change, sizeof(update->status_change));
    apl_db_copy_column_text(statement, 5, update->status_change_key,
                            sizeof(update->status_change_key));
    apl_db_copy_column_text(statement, 6, update->notes, sizeof(update->notes));
    apl_db_copy_column_text(statement, 7, update->created_at, sizeof(update->created_at));
}

static apl_status prepare(sqlite3 *const handle, const char *const sql, sqlite3_stmt **const out,
                          apl_error *const error)
{
    if (sqlite3_prepare_v2(handle, sql, -1, out, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare project update operation: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    return APL_OK;
}

static apl_status write_error(sqlite3 *const handle, apl_error *const error)
{
    const int extended = sqlite3_extended_errcode(handle);
    if (extended == SQLITE_CONSTRAINT_FOREIGNKEY) {
        apl_error_set(error, APL_ERR_NOT_FOUND, "the parent project no longer exists");
        return APL_ERR_NOT_FOUND;
    }
    if (extended == SQLITE_CONSTRAINT_CHECK || extended == SQLITE_CONSTRAINT_TRIGGER) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "project update data violates a stored rule: %s", sqlite3_errmsg(handle));
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_error_set(error, APL_ERR_DB_QUERY, "project update database operation failed: %s",
                  sqlite3_errmsg(handle));
    return APL_ERR_DB_QUERY;
}

apl_status apl_project_update_repository_append(const apl_db_connection *const connection,
                                                const int64_t team_id,
                                                const apl_project_update *const update,
                                                apl_project_update *const out_update,
                                                apl_error *const error)
{
    if (connection == nullptr || update == nullptr || out_update == nullptr || team_id <= 0 ||
        update->project_id <= 0 || update->update_date[0] == '\0' || update->summary[0] == '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "project update append arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_update = (apl_project_update){};
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    apl_status status =
        prepare(handle,
                "INSERT INTO project_updates (project_id, update_date, summary, status_change, "
                "status_change_key, notes, created_at) SELECT ?1, ?2, ?3, ?4, ?5, ?6, "
                "strftime('%Y-%m-%dT%H:%M:%fZ', 'now') "
                "WHERE EXISTS (SELECT 1 FROM projects WHERE projects.id = ?1 AND "
                "projects.team_id = ?7) "
                "RETURNING id, project_id, update_date, summary, status_change, status_change_key, "
                "notes, created_at;",
                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    int bind_status = sqlite3_bind_int64(statement, 1, update->project_id);
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 2, update->update_date, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 3, update->summary, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 4, update->status_change, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status =
            sqlite3_bind_text(statement, 5, update->status_change_key, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 6, update->notes, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_int64(statement, 7, team_id);
    }
    if (bind_status != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind project update: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }

    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "project %lld was not found for this team",
                      (long long)update->project_id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        status = write_error(handle, error);
        sqlite3_finalize(statement);
        return status;
    }
    read_update(statement, out_update);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_project_update_repository_get(const apl_db_connection *const connection,
                                             const int64_t team_id, const int64_t id,
                                             apl_project_update *const out_update,
                                             apl_error *const error)
{
    if (connection == nullptr || out_update == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "project update get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_update = (apl_project_update){};
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    const apl_status status = prepare(handle,
                                      "SELECT " UPDATE_COLUMNS " FROM project_updates u "
                                      "JOIN projects p ON p.id = u.project_id "
                                      "WHERE u.id = ?1 AND p.team_id = ?2;",
                                      &statement, error);
    if (status != APL_OK) {
        return status;
    }
    sqlite3_bind_int64(statement, 1, id);
    sqlite3_bind_int64(statement, 2, team_id);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "project update %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read project update: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_update(statement, out_update);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_project_update_repository_list(const apl_db_connection *const connection,
                                              const int64_t team_id, const int64_t project_id,
                                              apl_project_update **const out_updates,
                                              size_t *const out_count, apl_error *const error)
{
    if (connection == nullptr || out_updates == nullptr || out_count == nullptr || team_id <= 0 ||
        project_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "project update list arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_updates = nullptr;
    *out_count = 0;
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    apl_status status = prepare(handle,
                                "SELECT " UPDATE_COLUMNS " FROM project_updates u "
                                "JOIN projects p ON p.id = u.project_id "
                                "WHERE u.project_id = ?1 AND p.team_id = ?2 "
                                "ORDER BY u.update_date ASC, u.id ASC;",
                                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    sqlite3_bind_int64(statement, 1, project_id);
    sqlite3_bind_int64(statement, 2, team_id);

    apl_project_update *updates = nullptr;
    size_t count = 0;
    size_t capacity = 0;
    for (;;) {
        const int step_status = sqlite3_step(statement);
        if (step_status == SQLITE_DONE) {
            break;
        }
        if (step_status != SQLITE_ROW) {
            apl_error_set(error, APL_ERR_DB_QUERY, "cannot read project update list: %s",
                          sqlite3_errmsg(handle));
            status = APL_ERR_DB_QUERY;
            break;
        }
        if (count == capacity) {
            typeof(updates) const grown = apl_grow_array(updates, &capacity, 8, sizeof(*updates));
            if (grown == nullptr) {
                apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate project update list");
                status = APL_ERR_OUT_OF_MEMORY;
                break;
            }
            updates = grown;
        }
        read_update(statement, &updates[count]);
        ++count;
    }
    sqlite3_finalize(statement);
    if (status != APL_OK) {
        free(updates);
        return status;
    }
    *out_updates = updates;
    *out_count = count;
    return APL_OK;
}

void apl_project_update_repository_free_list(apl_project_update *const updates)
{
    free(updates);
}
