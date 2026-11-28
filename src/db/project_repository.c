#include "db/project_repository.h"

#include <sqlite3.h>
#include <stdlib.h>

#include "core/util.h"
#include "db/sqlite_util.h"

#define PROJECT_COLUMNS                                                                            \
    "id, team_id, season_id, car_id, title, category, category_key, description, status, "         \
    "status_key, target_date, actual_date, notes, archived, created_at, updated_at, archived_at"

static void bind_optional_text(sqlite3_stmt *const statement, const int index,
                               const char *const value)
{
    if (value == nullptr || value[0] == '\0') {
        (void)sqlite3_bind_null(statement, index);
    } else {
        (void)sqlite3_bind_text(statement, index, value, -1, SQLITE_TRANSIENT);
    }
}

static int bind_optional_id(sqlite3_stmt *const statement, const int index, const int64_t id)
{
    if (id > 0) {
        return sqlite3_bind_int64(statement, index, id);
    }
    return sqlite3_bind_null(statement, index);
}

static void read_project(sqlite3_stmt *const statement, apl_project *const project)
{
    *project = (apl_project){};
    project->id = sqlite3_column_int64(statement, 0);
    project->team_id = sqlite3_column_int64(statement, 1);
    project->season_id =
        sqlite3_column_type(statement, 2) == SQLITE_NULL ? 0 : sqlite3_column_int64(statement, 2);
    project->car_id =
        sqlite3_column_type(statement, 3) == SQLITE_NULL ? 0 : sqlite3_column_int64(statement, 3);
    apl_db_copy_column_text(statement, 4, project->title, sizeof(project->title));
    apl_db_copy_column_text(statement, 5, project->category, sizeof(project->category));
    apl_db_copy_column_text(statement, 6, project->category_key, sizeof(project->category_key));
    apl_db_copy_column_text(statement, 7, project->description, sizeof(project->description));
    apl_db_copy_column_text(statement, 8, project->status, sizeof(project->status));
    apl_db_copy_column_text(statement, 9, project->status_key, sizeof(project->status_key));
    apl_db_copy_column_text(statement, 10, project->target_date, sizeof(project->target_date));
    apl_db_copy_column_text(statement, 11, project->actual_date, sizeof(project->actual_date));
    apl_db_copy_column_text(statement, 12, project->notes, sizeof(project->notes));
    project->meta.archived = sqlite3_column_int(statement, 13) != 0;
    apl_db_copy_column_text(statement, 14, project->meta.created_at,
                            sizeof(project->meta.created_at));
    apl_db_copy_column_text(statement, 15, project->meta.updated_at,
                            sizeof(project->meta.updated_at));
    apl_db_copy_column_text(statement, 16, project->meta.archived_at,
                            sizeof(project->meta.archived_at));
}

static apl_status prepare(sqlite3 *const handle, const char *const sql, sqlite3_stmt **const out,
                          apl_error *const error)
{
    if (sqlite3_prepare_v2(handle, sql, -1, out, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare project operation: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    return APL_OK;
}

static apl_status write_error(sqlite3 *const handle, apl_error *const error)
{
    const int extended = sqlite3_extended_errcode(handle);
    if (extended == SQLITE_CONSTRAINT_UNIQUE || extended == SQLITE_CONSTRAINT_PRIMARYKEY) {
        apl_error_set(error, APL_ERR_CONFLICT, "the project conflicts with an existing project");
        return APL_ERR_CONFLICT;
    }
    if (extended == SQLITE_CONSTRAINT_FOREIGNKEY) {
        apl_error_set(error, APL_ERR_NOT_FOUND, "the owning team no longer exists");
        return APL_ERR_NOT_FOUND;
    }
    if (extended == SQLITE_CONSTRAINT_CHECK || extended == SQLITE_CONSTRAINT_TRIGGER) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "project data violates a stored rule: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_error_set(error, APL_ERR_DB_QUERY, "project database operation failed: %s",
                  sqlite3_errmsg(handle));
    return APL_ERR_DB_QUERY;
}

static int bind_project_fields(sqlite3_stmt *const statement, const int first_index,
                               const apl_project *const project)
{
    int index = first_index;
    int status = bind_optional_id(statement, index++, project->season_id);
    if (status == SQLITE_OK) {
        status = bind_optional_id(statement, index++, project->car_id);
    }
    if (status == SQLITE_OK) {
        status = sqlite3_bind_text(statement, index++, project->title, -1, SQLITE_TRANSIENT);
    }
    if (status == SQLITE_OK) {
        status = sqlite3_bind_text(statement, index++, project->category, -1, SQLITE_TRANSIENT);
    }
    if (status == SQLITE_OK) {
        status = sqlite3_bind_text(statement, index++, project->category_key, -1, SQLITE_TRANSIENT);
    }
    if (status == SQLITE_OK) {
        status = sqlite3_bind_text(statement, index++, project->description, -1, SQLITE_TRANSIENT);
    }
    if (status == SQLITE_OK) {
        status = sqlite3_bind_text(statement, index++, project->status, -1, SQLITE_TRANSIENT);
    }
    if (status == SQLITE_OK) {
        status = sqlite3_bind_text(statement, index++, project->status_key, -1, SQLITE_TRANSIENT);
    }
    if (status == SQLITE_OK) {
        bind_optional_text(statement, index++, project->target_date);
    }
    if (status == SQLITE_OK) {
        bind_optional_text(statement, index++, project->actual_date);
    }
    if (status == SQLITE_OK) {
        status = sqlite3_bind_text(statement, index, project->notes, -1, SQLITE_TRANSIENT);
    }
    return status;
}

static apl_status write_and_read(sqlite3 *const handle, const char *const sql,
                                 const int64_t team_id, const int64_t id, const bool bind_id,
                                 const apl_project *const project, apl_project *const out_project,
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
        bind_status = bind_project_fields(statement, fields_index, project);
    }
    if (bind_status != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind project operation: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }

    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE && bind_id) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "project %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        status = write_error(handle, error);
        sqlite3_finalize(statement);
        return status;
    }
    read_project(statement, out_project);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_project_repository_create(const apl_db_connection *const connection,
                                         const apl_project *const project,
                                         apl_project *const out_project, apl_error *const error)
{
    if (connection == nullptr || project == nullptr || out_project == nullptr ||
        project->team_id <= 0 || project->title[0] == '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "project create arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_project = (apl_project){};
    return write_and_read(
        handle,
        "INSERT INTO projects (team_id, season_id, car_id, title, category, category_key, "
        "description, status, status_key, target_date, actual_date, notes, archived, created_at, "
        "updated_at) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, 0, "
        "strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), strftime('%Y-%m-%dT%H:%M:%fZ', 'now')) "
        "RETURNING " PROJECT_COLUMNS ";",
        project->team_id, 0, false, project, out_project, error);
}

apl_status apl_project_repository_get(const apl_db_connection *const connection,
                                      const int64_t team_id, const int64_t id,
                                      apl_project *const out_project, apl_error *const error)
{
    if (connection == nullptr || out_project == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "project get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_project = (apl_project){};

    sqlite3_stmt *statement = nullptr;
    const apl_status status =
        prepare(handle, "SELECT " PROJECT_COLUMNS " FROM projects WHERE id = ?1 AND team_id = ?2;",
                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    sqlite3_bind_int64(statement, 1, id);
    sqlite3_bind_int64(statement, 2, team_id);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "project %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read project: %s", sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_project(statement, out_project);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_project_repository_update(const apl_db_connection *const connection,
                                         const apl_project *const project,
                                         apl_project *const out_project, apl_error *const error)
{
    if (connection == nullptr || project == nullptr || out_project == nullptr ||
        project->team_id <= 0 || project->id <= 0 || project->title[0] == '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "project update arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_project = (apl_project){};
    return write_and_read(
        handle,
        "UPDATE projects SET season_id = ?3, car_id = ?4, title = ?5, category = ?6, "
        "category_key = ?7, description = ?8, status = ?9, status_key = ?10, target_date = ?11, "
        "actual_date = ?12, notes = ?13, "
        "updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now') "
        "WHERE id = ?2 AND team_id = ?1 RETURNING " PROJECT_COLUMNS ";",
        project->team_id, project->id, true, project, out_project, error);
}

apl_status apl_project_repository_archive(const apl_db_connection *const connection,
                                          const int64_t team_id, const int64_t id,
                                          apl_project *const out_project, apl_error *const error)
{
    if (connection == nullptr || out_project == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "project archive arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_project = (apl_project){};

    sqlite3_stmt *statement = nullptr;
    const apl_status status =
        prepare(handle,
                "UPDATE projects SET archived = 1, "
                "archived_at = COALESCE(archived_at, strftime('%Y-%m-%dT%H:%M:%fZ', 'now')), "
                "updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now') "
                "WHERE id = ?2 AND team_id = ?1 RETURNING " PROJECT_COLUMNS ";",
                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    sqlite3_bind_int64(statement, 1, team_id);
    sqlite3_bind_int64(statement, 2, id);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "project %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot archive project: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_project(statement, out_project);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_project_repository_list(const apl_db_connection *const connection,
                                       const apl_project_filter *const filter,
                                       apl_project **const out_projects, size_t *const out_count,
                                       apl_error *const error)
{
    if (connection == nullptr || out_projects == nullptr || out_count == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "project list arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_projects = nullptr;
    *out_count = 0;
    if (filter == nullptr || filter->team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "project list requires a team scope");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    apl_status status = prepare(handle,
                                "SELECT " PROJECT_COLUMNS " FROM projects WHERE team_id = ?1 "
                                "AND (?2 = 1 OR archived = 0) "
                                "AND (?3 = '' OR status_key = ?3) "
                                "AND (?4 = 0 OR season_id = ?4) "
                                "AND (?5 = '' OR category_key = ?5) "
                                "ORDER BY archived ASC, title COLLATE NOCASE ASC, id ASC;",
                                &statement, error);
    if (status != APL_OK) {
        return status;
    }
    int bind_status = sqlite3_bind_int64(statement, 1, filter->team_id);
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_int(statement, 2, filter->include_archived ? 1 : 0);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 3, filter->status_key, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_int64(statement, 4, filter->season_id);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 5, filter->category_key, -1, SQLITE_TRANSIENT);
    }
    if (bind_status != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind project filters: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }

    apl_project *projects = nullptr;
    size_t count = 0;
    size_t capacity = 0;
    for (;;) {
        const int step_status = sqlite3_step(statement);
        if (step_status == SQLITE_DONE) {
            break;
        }
        if (step_status != SQLITE_ROW) {
            apl_error_set(error, APL_ERR_DB_QUERY, "cannot read project list: %s",
                          sqlite3_errmsg(handle));
            status = APL_ERR_DB_QUERY;
            break;
        }
        if (count == capacity) {
            typeof(projects) const grown =
                apl_grow_array(projects, &capacity, 8, sizeof(*projects));
            if (grown == nullptr) {
                apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate project list");
                status = APL_ERR_OUT_OF_MEMORY;
                break;
            }
            projects = grown;
        }
        read_project(statement, &projects[count]);
        ++count;
    }
    sqlite3_finalize(statement);
    if (status != APL_OK) {
        free(projects);
        return status;
    }
    *out_projects = projects;
    *out_count = count;
    return APL_OK;
}

void apl_project_repository_free_list(apl_project *const projects)
{
    free(projects);
}
