#include "db/membership_repository.h"

#include <sqlite3.h>
#include <stdlib.h>

#include "core/util.h"
#include "db/sqlite_util.h"

#define MEMBERSHIP_COLUMNS                                                                         \
    "id, team_id, driver_id, start_date, end_date, role, notes, created_at, updated_at"

static void bind_optional_text(sqlite3_stmt *const statement, const int index,
                               const char *const value)
{
    if (value == nullptr || value[0] == '\0') {
        (void)sqlite3_bind_null(statement, index);
    } else {
        (void)sqlite3_bind_text(statement, index, value, -1, SQLITE_TRANSIENT);
    }
}

static void read_membership(sqlite3_stmt *const statement, apl_membership *const membership)
{
    membership->id = sqlite3_column_int64(statement, 0);
    membership->team_id = sqlite3_column_int64(statement, 1);
    membership->driver_id = sqlite3_column_int64(statement, 2);
    apl_db_copy_column_text(statement, 3, membership->start_date, sizeof(membership->start_date));
    apl_db_copy_column_text(statement, 4, membership->end_date, sizeof(membership->end_date));
    apl_db_copy_column_text(statement, 5, membership->role, sizeof(membership->role));
    apl_db_copy_column_text(statement, 6, membership->notes, sizeof(membership->notes));
    apl_db_copy_column_text(statement, 7, membership->created_at, sizeof(membership->created_at));
    apl_db_copy_column_text(statement, 8, membership->updated_at, sizeof(membership->updated_at));
}

static apl_status write_error(sqlite3 *const handle, apl_error *const error)
{
    const int extended = sqlite3_extended_errcode(handle);
    if (extended == SQLITE_CONSTRAINT_UNIQUE || extended == SQLITE_CONSTRAINT_PRIMARYKEY) {
        apl_error_set(error, APL_ERR_CONFLICT,
                      "this exact team-driver membership period already exists");
        return APL_ERR_CONFLICT;
    }
    if (extended == SQLITE_CONSTRAINT_FOREIGNKEY) {
        apl_error_set(error, APL_ERR_NOT_FOUND, "the referenced team or driver no longer exists");
        return APL_ERR_NOT_FOUND;
    }
    if (extended == SQLITE_CONSTRAINT_CHECK) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "membership data violates a stored constraint: %s", sqlite3_errmsg(handle));
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_error_set(error, APL_ERR_DB_QUERY, "membership database operation failed: %s",
                  sqlite3_errmsg(handle));
    return APL_ERR_DB_QUERY;
}

static apl_status create_and_read(sqlite3 *const handle, const int64_t team_id,
                                  const int64_t driver_id, const char *const start_date,
                                  const char *const end_date, const char *const role,
                                  const char *const notes, apl_membership *const out_membership,
                                  apl_error *const error)
{
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(
            handle,
            "INSERT INTO team_driver_memberships (team_id, driver_id, start_date, end_date, role, "
            "notes, created_at, updated_at) VALUES (?1, ?2, ?3, ?4, ?5, ?6, "
            "strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), "
            "strftime('%Y-%m-%dT%H:%M:%fZ', 'now')) RETURNING " MEMBERSHIP_COLUMNS ";",
            -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare membership create: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    int bind_status = sqlite3_bind_int64(statement, 1, team_id);
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_int64(statement, 2, driver_id);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 3, start_date, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_optional_text(statement, 4, end_date);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 5, role, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 6, notes, -1, SQLITE_TRANSIENT);
    }
    if (bind_status != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind membership create: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    const int step_status = sqlite3_step(statement);
    if (step_status != SQLITE_ROW) {
        const apl_status status = write_error(handle, error);
        sqlite3_finalize(statement);
        return status;
    }
    read_membership(statement, out_membership);
    sqlite3_finalize(statement);
    return APL_OK;
}

static apl_status update_and_read(sqlite3 *const handle, const char *const sql,
                                  const int64_t team_id, const int64_t id,
                                  const char *const start_date, const char *const end_date,
                                  const char *const role, const char *const notes,
                                  apl_membership *const out_membership, apl_error *const error)
{
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle, sql, -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare membership update: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    int bind_status = sqlite3_bind_int64(statement, 1, team_id);
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_int64(statement, 2, id);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 3, start_date, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_optional_text(statement, 4, end_date);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 5, role, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, 6, notes, -1, SQLITE_TRANSIENT);
    }
    if (bind_status != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind membership update: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "membership %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        const apl_status status = write_error(handle, error);
        sqlite3_finalize(statement);
        return status;
    }
    read_membership(statement, out_membership);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_membership_repository_create(
    const apl_db_connection *const connection, const int64_t team_id, const int64_t driver_id,
    const char *const start_date, const char *const end_date, const char *const role,
    const char *const notes, apl_membership *const out_membership, apl_error *const error)
{
    if (connection == nullptr || start_date == nullptr || end_date == nullptr || role == nullptr ||
        notes == nullptr || out_membership == nullptr || team_id <= 0 || driver_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "membership create arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_membership = (apl_membership){};
    return create_and_read(handle, team_id, driver_id, start_date, end_date, role, notes,
                           out_membership, error);
}

apl_status apl_membership_repository_get(const apl_db_connection *const connection,
                                         const int64_t team_id, const int64_t id,
                                         apl_membership *const out_membership,
                                         apl_error *const error)
{
    if (connection == nullptr || out_membership == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "membership get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_membership = (apl_membership){};

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle,
                           "SELECT " MEMBERSHIP_COLUMNS
                           " FROM team_driver_memberships WHERE id = ?1 AND team_id = ?2;",
                           -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare membership lookup: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(statement, 1, id);
    sqlite3_bind_int64(statement, 2, team_id);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "membership %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read membership: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_membership(statement, out_membership);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_membership_repository_update(
    const apl_db_connection *const connection, const int64_t team_id, const int64_t id,
    const char *const start_date, const char *const end_date, const char *const role,
    const char *const notes, apl_membership *const out_membership, apl_error *const error)
{
    if (connection == nullptr || start_date == nullptr || end_date == nullptr || role == nullptr ||
        notes == nullptr || out_membership == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "membership update arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_membership = (apl_membership){};
    return update_and_read(
        handle,
        "UPDATE team_driver_memberships SET start_date = ?3, end_date = ?4, role = ?5, notes = ?6, "
        "updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now') "
        "WHERE id = ?2 AND team_id = ?1 RETURNING " MEMBERSHIP_COLUMNS ";",
        team_id, id, start_date, end_date, role, notes, out_membership, error);
}

apl_status apl_membership_repository_end(const apl_db_connection *const connection,
                                         const int64_t team_id, const int64_t id,
                                         const char *const end_date,
                                         apl_membership *const out_membership,
                                         apl_error *const error)
{
    if (connection == nullptr || end_date == nullptr || out_membership == nullptr ||
        end_date[0] == '\0' || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "membership end arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_membership = (apl_membership){};

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle,
                           "UPDATE team_driver_memberships SET end_date = ?3, "
                           "updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now') "
                           "WHERE id = ?2 AND team_id = ?1 RETURNING " MEMBERSHIP_COLUMNS ";",
                           -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare membership end: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(statement, 1, team_id);
    sqlite3_bind_int64(statement, 2, id);
    sqlite3_bind_text(statement, 3, end_date, -1, SQLITE_TRANSIENT);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "membership %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        const apl_status status = write_error(handle, error);
        sqlite3_finalize(statement);
        return status;
    }
    read_membership(statement, out_membership);
    sqlite3_finalize(statement);
    return APL_OK;
}

static apl_status collect_memberships(sqlite3 *const handle, sqlite3_stmt *const statement,
                                      apl_membership **const out_memberships,
                                      size_t *const out_count, apl_error *const error)
{
    apl_membership *memberships = nullptr;
    size_t count = 0;
    size_t capacity = 0;
    apl_status status = APL_OK;
    for (;;) {
        const int step_status = sqlite3_step(statement);
        if (step_status == SQLITE_DONE) {
            break;
        }
        if (step_status != SQLITE_ROW) {
            apl_error_set(error, APL_ERR_DB_QUERY, "cannot read membership list: %s",
                          sqlite3_errmsg(handle));
            status = APL_ERR_DB_QUERY;
            break;
        }
        if (count == capacity) {
            typeof(memberships) const grown =
                apl_grow_array(memberships, &capacity, 8, sizeof(*memberships));
            if (grown == nullptr) {
                apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate membership list");
                status = APL_ERR_OUT_OF_MEMORY;
                break;
            }
            memberships = grown;
        }
        read_membership(statement, &memberships[count]);
        ++count;
    }
    if (status != APL_OK) {
        free(memberships);
        return status;
    }
    *out_memberships = memberships;
    *out_count = count;
    return APL_OK;
}

apl_status apl_membership_repository_list_for_team(const apl_db_connection *const connection,
                                                   const int64_t team_id, const bool include_ended,
                                                   apl_membership **const out_memberships,
                                                   size_t *const out_count, apl_error *const error)
{
    if (connection == nullptr || out_memberships == nullptr || out_count == nullptr ||
        team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "membership list arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_memberships = nullptr;
    *out_count = 0;
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle,
                           "SELECT " MEMBERSHIP_COLUMNS " FROM team_driver_memberships "
                           "WHERE team_id = ?1 AND (?2 = 1 OR end_date IS NULL) "
                           "ORDER BY (end_date IS NOT NULL) ASC, start_date DESC, id DESC;",
                           -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare membership list: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(statement, 1, team_id);
    sqlite3_bind_int(statement, 2, include_ended ? 1 : 0);
    const apl_status status =
        collect_memberships(handle, statement, out_memberships, out_count, error);
    sqlite3_finalize(statement);
    return status;
}

apl_status apl_membership_repository_list_for_driver(const apl_db_connection *const connection,
                                                     const int64_t driver_id,
                                                     apl_membership **const out_memberships,
                                                     size_t *const out_count,
                                                     apl_error *const error)
{
    if (connection == nullptr || out_memberships == nullptr || out_count == nullptr ||
        driver_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "driver membership list arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_memberships = nullptr;
    *out_count = 0;
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle,
                           "SELECT " MEMBERSHIP_COLUMNS " FROM team_driver_memberships "
                           "WHERE driver_id = ?1 ORDER BY start_date ASC, id ASC;",
                           -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare driver membership list: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(statement, 1, driver_id);
    const apl_status status =
        collect_memberships(handle, statement, out_memberships, out_count, error);
    sqlite3_finalize(statement);
    return status;
}

void apl_membership_repository_free_list(apl_membership *const memberships)
{
    free(memberships);
}
