#include "db/car_repository.h"

#include <sqlite3.h>
#include <stdlib.h>

#include "core/util.h"
#include "db/sqlite_util.h"

#define CAR_COLUMNS                                                                                \
    "id, team_id, label, chassis, notes, archived, created_at, updated_at, archived_at"

static void read_car(sqlite3_stmt *const statement, apl_car *const car)
{
    car->id = sqlite3_column_int64(statement, 0);
    car->team_id = sqlite3_column_int64(statement, 1);
    apl_db_copy_column_text(statement, 2, car->label, sizeof(car->label));
    apl_db_copy_column_text(statement, 3, car->chassis, sizeof(car->chassis));
    apl_db_copy_column_text(statement, 4, car->notes, sizeof(car->notes));
    car->archived = sqlite3_column_int(statement, 5) != 0;
    apl_db_copy_column_text(statement, 6, car->created_at, sizeof(car->created_at));
    apl_db_copy_column_text(statement, 7, car->updated_at, sizeof(car->updated_at));
    apl_db_copy_column_text(statement, 8, car->archived_at, sizeof(car->archived_at));
}

static apl_status write_error(sqlite3 *const handle, const char *const label,
                              apl_error *const error)
{
    const int extended = sqlite3_extended_errcode(handle);
    if (extended == SQLITE_CONSTRAINT_UNIQUE || extended == SQLITE_CONSTRAINT_PRIMARYKEY) {
        apl_error_set(error, APL_ERR_CONFLICT, "a car labelled '%s' already exists for this team",
                      label);
        return APL_ERR_CONFLICT;
    }
    if (extended == SQLITE_CONSTRAINT_FOREIGNKEY) {
        apl_error_set(error, APL_ERR_NOT_FOUND, "the owning team no longer exists");
        return APL_ERR_NOT_FOUND;
    }
    if (extended == SQLITE_CONSTRAINT_CHECK) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "car data violates a stored constraint: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_error_set(error, APL_ERR_DB_QUERY, "car database operation failed: %s",
                  sqlite3_errmsg(handle));
    return APL_ERR_DB_QUERY;
}

static apl_status write_and_read(sqlite3 *const handle, const char *const sql,
                                 const int64_t team_id, const int64_t id, const bool bind_id,
                                 const char *const label, const char *const chassis,
                                 const char *const notes, apl_car *const out_car,
                                 apl_error *const error)
{
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle, sql, -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare car operation: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    int bind_status = sqlite3_bind_int64(statement, 1, team_id);
    int next = 2;
    if (bind_id && bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_int64(statement, next++, id);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, next++, label, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, next++, chassis, -1, SQLITE_TRANSIENT);
    }
    if (bind_status == SQLITE_OK) {
        bind_status = sqlite3_bind_text(statement, next, notes, -1, SQLITE_TRANSIENT);
    }
    if (bind_status != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot bind car operation: %s",
                      sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }

    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE && bind_id) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "car %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        const apl_status status = write_error(handle, label, error);
        sqlite3_finalize(statement);
        return status;
    }
    read_car(statement, out_car);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_car_repository_create(const apl_db_connection *const connection,
                                     const int64_t team_id, const char *const label,
                                     const char *const chassis, const char *const notes,
                                     apl_car *const out_car, apl_error *const error)
{
    if (connection == nullptr || label == nullptr || chassis == nullptr || notes == nullptr ||
        out_car == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "car create arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_car = (apl_car){};
    return write_and_read(
        handle,
        "INSERT INTO cars (team_id, label, chassis, notes, archived, created_at, updated_at) "
        "VALUES (?1, ?2, ?3, ?4, 0, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'), "
        "strftime('%Y-%m-%dT%H:%M:%fZ', 'now')) RETURNING " CAR_COLUMNS ";",
        team_id, 0, false, label, chassis, notes, out_car, error);
}

apl_status apl_car_repository_get(const apl_db_connection *const connection, const int64_t team_id,
                                  const int64_t id, apl_car *const out_car, apl_error *const error)
{
    if (connection == nullptr || out_car == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "car get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_car = (apl_car){};
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle,
                           "SELECT " CAR_COLUMNS " FROM cars WHERE id = ?1 AND team_id = ?2;", -1,
                           &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare car lookup: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(statement, 1, id);
    sqlite3_bind_int64(statement, 2, team_id);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "car %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read car: %s", sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_car(statement, out_car);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_car_repository_update(const apl_db_connection *const connection,
                                     const int64_t team_id, const int64_t id,
                                     const char *const label, const char *const chassis,
                                     const char *const notes, apl_car *const out_car,
                                     apl_error *const error)
{
    if (connection == nullptr || label == nullptr || chassis == nullptr || notes == nullptr ||
        out_car == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "car update arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_car = (apl_car){};
    return write_and_read(handle,
                          "UPDATE cars SET label = ?3, chassis = ?4, notes = ?5, "
                          "updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now') "
                          "WHERE id = ?2 AND team_id = ?1 RETURNING " CAR_COLUMNS ";",
                          team_id, id, true, label, chassis, notes, out_car, error);
}

apl_status apl_car_repository_archive(const apl_db_connection *const connection,
                                      const int64_t team_id, const int64_t id,
                                      apl_car *const out_car, apl_error *const error)
{
    if (connection == nullptr || out_car == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "car archive arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_car = (apl_car){};
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(
            handle,
            "UPDATE cars SET archived = 1, "
            "archived_at = COALESCE(archived_at, strftime('%Y-%m-%dT%H:%M:%fZ', 'now')), "
            "updated_at = strftime('%Y-%m-%dT%H:%M:%fZ', 'now') "
            "WHERE id = ?2 AND team_id = ?1 RETURNING " CAR_COLUMNS ";",
            -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare car archive: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(statement, 1, team_id);
    sqlite3_bind_int64(statement, 2, id);
    const int step_status = sqlite3_step(statement);
    if (step_status == SQLITE_DONE) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_NOT_FOUND, "car %lld was not found for this team",
                      (long long)id);
        return APL_ERR_NOT_FOUND;
    }
    if (step_status != SQLITE_ROW) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot archive car: %s", sqlite3_errmsg(handle));
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    read_car(statement, out_car);
    sqlite3_finalize(statement);
    return APL_OK;
}

apl_status apl_car_repository_list(const apl_db_connection *const connection, const int64_t team_id,
                                   const bool include_archived, apl_car **const out_cars,
                                   size_t *const out_count, apl_error *const error)
{
    if (connection == nullptr || out_cars == nullptr || out_count == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "car list arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_cars = nullptr;
    *out_count = 0;
    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle,
                           "SELECT " CAR_COLUMNS " FROM cars WHERE team_id = ?1 "
                           "AND (?2 = 1 OR archived = 0) "
                           "ORDER BY archived ASC, label COLLATE NOCASE ASC, id ASC;",
                           -1, &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot prepare car list: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_int64(statement, 1, team_id);
    sqlite3_bind_int(statement, 2, include_archived ? 1 : 0);

    apl_car *cars = nullptr;
    size_t count = 0;
    size_t capacity = 0;
    apl_status status = APL_OK;
    for (;;) {
        const int step_status = sqlite3_step(statement);
        if (step_status == SQLITE_DONE) {
            break;
        }
        if (step_status != SQLITE_ROW) {
            apl_error_set(error, APL_ERR_DB_QUERY, "cannot read car list: %s",
                          sqlite3_errmsg(handle));
            status = APL_ERR_DB_QUERY;
            break;
        }
        if (count == capacity) {
            typeof(cars) const grown = apl_grow_array(cars, &capacity, 8, sizeof(*cars));
            if (grown == nullptr) {
                apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate car list");
                status = APL_ERR_OUT_OF_MEMORY;
                break;
            }
            cars = grown;
        }
        read_car(statement, &cars[count]);
        ++count;
    }
    sqlite3_finalize(statement);
    if (status != APL_OK) {
        free(cars);
        return status;
    }
    *out_cars = cars;
    *out_count = count;
    return APL_OK;
}

void apl_car_repository_free_list(apl_car *const cars)
{
    free(cars);
}
