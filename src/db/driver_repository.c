#include "db/driver_repository.h"

#include <sqlite3.h>
#include <stdlib.h>

#include "core/util.h"
#include "db/sqlite_util.h"

#define DRIVER_COLUMNS "id, name, short_name, notes, archived, created_at, updated_at, archived_at"

static void read_driver(sqlite3_stmt *const statement, apl_driver *const driver)
{
    driver->id = sqlite3_column_int64(statement, 0);
    apl_db_copy_column_text(statement, 1, driver->name, sizeof(driver->name));
    apl_db_copy_column_text(statement, 2, driver->short_name, sizeof(driver->short_name));
    apl_db_copy_column_text(statement, 3, driver->notes, sizeof(driver->notes));
    driver->meta.archived = sqlite3_column_int(statement, 4) != 0;
    apl_db_copy_column_text(statement, 5, driver->meta.created_at, sizeof(driver->meta.created_at));
    apl_db_copy_column_text(statement, 6, driver->meta.updated_at, sizeof(driver->meta.updated_at));
    apl_db_copy_column_text(statement, 7, driver->meta.archived_at,
                            sizeof(driver->meta.archived_at));
}

static apl_status write_error(sqlite3 *const handle, apl_error *const error)
{
    const int extended = sqlite3_extended_errcode(handle);
    if (extended == SQLITE_CONSTRAINT_CHECK) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "driver data violates a stored constraint: %s", sqlite3_errmsg(handle));
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_error_set(error, APL_ERR_DB_QUERY, "driver database operation failed: %s",
                  sqlite3_errmsg(handle));
    return APL_ERR_DB_QUERY;
}

static apl_status write_and_read(sqlite3 *const handle, const char *const sql,
                                 const char *const name, const char *const short_name,
                                 const char *const notes, const int64_t id, const bool bind_id,
                                 apl_driver *const out_driver, apl_error *const error)
{
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle, sql, -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare driver operation: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    int bind_status = SQLITE_OK;
    int next = 1;
    if (bind_id) {
        bind_status = sqlite3_bind_int64(statement, next++, id);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, next++, name, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, next++, short_name, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, next, notes, -1, SQLITE_TRANSIENT);
    }
    if (bind_status != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind driver operation: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }

    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE && bind_id) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "driver %lld was not found", (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        const apl_status status = write_error(handle, error);
        sqlite3_finalize(statement);
        return status;
    }
    read_driver(statement, out_driver);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_driver_repository_create(const apl_db_connection *const connection,
                                        const char *const name, const char *const short_name,
                                        const char *const notes, apl_driver *const out_driver,
                                        apl_error *const error)
{
    if (connection == nullptr || name == nullptr || short_name == nullptr || notes == nullptr ||
        out_driver == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "driver create arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_driver = (apl_driver){};
    return write_and_read(
        handle,
        "INSERT INTO drivers (name, short_name, notes, archived, created_at, updated_at) "
        "VALUES (?1, ?2, ?3, 0, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), "
        "strftime('%Y-%m-%dT%H:%M:%fZ', 'now')) RETURNING " DRIVER_COLUMNS ";",
        name, short_name, notes, 0, false, out_driver, error);
}

apl_status apl_driver_repository_get(const apl_db_connection *const connection, const int64_t id,
                                     apl_driver *const out_driver, apl_error *const error)
{
    if (connection == nullptr || out_driver == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "driver get arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_driver = (apl_driver){};

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle, "SELECT " DRIVER_COLUMNS " FROM drivers WHERE id = ?1;", -1,
                           &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare driver lookup: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(statement, 1, id);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "driver %lld was not found", (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read driver: %s", sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_driver(statement, out_driver);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_driver_repository_update(const apl_db_connection *const connection, const int64_t id,
                                        const char *const name, const char *const short_name,
                                        const char *const notes, apl_driver *const out_driver,
                                        apl_error *const error)
{
    if (connection == nullptr || name == nullptr || short_name == nullptr || notes == nullptr ||
        out_driver == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "driver update arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_driver = (apl_driver){};
    return write_and_read(handle,
                          "UPDATE drivers SET name = ?2, short_name = ?3, notes = ?4, "
                          "updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now') "
                          "WHERE id = ?1 RETURNING " DRIVER_COLUMNS ";",
                          name, short_name, notes, id, true, out_driver, error);
}

apl_status apl_driver_repository_archive(const apl_db_connection *const connection,
                                         const int64_t id, apl_driver *const out_driver,
                                         apl_error *const error)
{
    if (connection == nullptr || out_driver == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "driver archive arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_driver = (apl_driver){};
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(
            handle,
            "UPDATE drivers SET archived = 1, "
            "archived_at = COALESCE(archived_at, strftime('%Y-%m-%dT%H:%M:%fZ', 'now')), "
            "updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now') "
            "WHERE id = ?1 RETURNING " DRIVER_COLUMNS ";",
            -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare driver archive: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(statement, 1, id);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "driver %lld was not found", (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot archive driver: %s", sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_driver(statement, out_driver);
    sqlite3_finalize(statement);
    return APL_OK;
}

/** Collect rows from a prepared driver query. */
static apl_status collect_drivers(sqlite3 *const handle, sqlite3_stmt *const statement,
                                  apl_driver **const out_drivers, size_t *const out_count,
                                  apl_error *const error)
{
    apl_driver *drivers = nullptr;
    size_t count = 0;
    size_t capacity = 0;
    apl_status status = APL_OK;
    for (;;) {
        const int step_status = sqlite3_step(statement);
        if (step_status == SQLITE_DONE) {
            break;
        }
        if (step_status != SQLITE_ROW) {
            apl_error_set(error, APL_ERR_DB_QUERY, "cannot read driver list: %s",
                          sqlite3_errmsg(handle));
            status = APL_ERR_DB_QUERY;
            break;
        }
        if (count == capacity) {
            typeof(drivers) const grown = apl_grow_array(drivers, &capacity, 8, sizeof(*drivers));
            if (grown == nullptr) {
                apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate driver list");
                status = APL_ERR_OUT_OF_MEMORY;
                break;
            }
            drivers = grown;
        }
        read_driver(statement, &drivers[count]);
        ++count;
    }
    if (status != APL_OK) {
        free(drivers);
        return status;
    }
    *out_drivers = drivers;
    *out_count = count;
    return APL_OK;
}

apl_status apl_driver_repository_list(const apl_db_connection *const connection,
                                      const bool include_archived, apl_driver **const out_drivers,
                                      size_t *const out_count, apl_error *const error)
{
    if (connection == nullptr || out_drivers == nullptr || out_count == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "driver list arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_drivers = nullptr;
    *out_count = 0;
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle,
                           "SELECT " DRIVER_COLUMNS " FROM drivers "
                           "WHERE (?1 = 1 OR archived = 0) "
                           "ORDER BY archived ASC, name COLLATE NOCASE ASC, id ASC;",
                           -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare driver list: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int(statement, 1, include_archived ? 1 : 0);
    const apl_status status = collect_drivers(handle, statement, out_drivers, out_count, error);
    sqlite3_finalize(statement);
    return status;
}

apl_status apl_driver_repository_list_for_team(const apl_db_connection *const connection,
                                               const int64_t team_id, const bool include_archived,
                                               apl_driver **const out_drivers,
                                               size_t *const out_count, apl_error *const error)
{
    if (connection == nullptr || out_drivers == nullptr || out_count == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "team driver list arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_drivers = nullptr;
    *out_count = 0;
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle,
                           "SELECT " DRIVER_COLUMNS " FROM drivers d "
                           "WHERE EXISTS (SELECT 1 FROM team_driver_memberships m "
                           "WHERE m.driver_id = d.id AND m.team_id = ?1) "
                           "AND (?2 = 1 OR d.archived = 0) "
                           "ORDER BY d.archived ASC, d.name COLLATE NOCASE ASC, d.id ASC;",
                           -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare team driver list: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(statement, 1, team_id);
    sqlite3_bind_int(statement, 2, include_archived ? 1 : 0);
    const apl_status status = collect_drivers(handle, statement, out_drivers, out_count, error);
    sqlite3_finalize(statement);
    return status;
}

void apl_driver_repository_free_list(apl_driver *const drivers)
{
    free(drivers);
}
