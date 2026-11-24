#include "db/migrations.h"

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#define SCHEMA_VERSION_TABLE "apl_schema_migrations"

static apl_status exec_sql(sqlite3 *const handle, const char *const sql, apl_error *const error)
{
    char *sqlite_error = nullptr;
    if (sqlite3_exec(handle, sql, nullptr, nullptr, &sqlite_error) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "%s",
                      (sqlite_error != nullptr) ? sqlite_error : "unknown SQL error");
        sqlite3_free(sqlite_error);
        return APL_ERR_DB_QUERY;
    }
    sqlite3_free(sqlite_error);
    return APL_OK;
}

/** Report whether a table exists. Returns APL_OK and stores the answer in
 *  *out_exists; a failed query returns APL_ERR_DB_QUERY. */
static apl_status table_exists(sqlite3 *const handle, const char *const name,
                               bool *const out_exists, apl_error *const error)
{
    *out_exists = false;
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle,
                           "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?1;", -1,
                           &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot inspect schema: %s", sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }
    sqlite3_bind_text(statement, 1, name, -1, SQLITE_STATIC);
    *out_exists = sqlite3_step(statement) == SQLITE_ROW;
    sqlite3_finalize(statement);
    return APL_OK;
}

/** Ensure the schema-version table exists. */
static apl_status ensure_version_table(sqlite3 *const handle, apl_error *const error)
{
    bool exists = false;
    const apl_status status = table_exists(handle, SCHEMA_VERSION_TABLE, &exists, error);
    if (status != APL_OK) {
        return status;
    }
    if (exists) {
        return APL_OK;
    }
    return exec_sql(handle,
                    "CREATE TABLE " SCHEMA_VERSION_TABLE " ("
                    "version INTEGER PRIMARY KEY,"
                    "applied_at TEXT NOT NULL);",
                    error);
}

apl_status apl_db_current_version(apl_db_connection *const connection, int *const out_version,
                                  apl_error *const error)
{
    if (out_version != nullptr) {
        *out_version = 0;
    }
    if (connection == nullptr || out_version == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "migration version arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }

    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    bool version_table_exists = false;
    const apl_status status =
        table_exists(handle, SCHEMA_VERSION_TABLE, &version_table_exists, error);
    if (status != APL_OK) {
        return status;
    }
    if (!version_table_exists) {
        *out_version = 0;
        return APL_OK;
    }

    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(handle,
                           "SELECT COALESCE(MAX(version), 0) FROM " SCHEMA_VERSION_TABLE ";", -1,
                           &statement, nullptr) != SQLITE_OK) {
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read schema version: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }

    if (sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement);
        apl_error_set(error, APL_ERR_DB_QUERY, "cannot read schema version: %s",
                      sqlite3_errmsg(handle));
        return APL_ERR_DB_QUERY;
    }

    *out_version = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return APL_OK;
}

/** Validate the compiled migration list before touching the database. */
static apl_status validate_list(const apl_db_migration *const migrations, const size_t count,
                                apl_error *const error)
{
    for (size_t index = 0; index < count; ++index) {
        if (migrations[index].version != (int)(index + 1)) {
            apl_error_set(
                error, APL_ERR_DB_MIGRATION,
                "compiled migration list is invalid at position %zu: expected version %zu, "
                "found %d (versions must be contiguous and ascending from 1)",
                index, index + 1, migrations[index].version);
            return APL_ERR_DB_MIGRATION;
        }
        if (migrations[index].sql == nullptr) {
            apl_error_set(error, APL_ERR_DB_MIGRATION, "compiled migration %d has no SQL",
                          migrations[index].version);
            return APL_ERR_DB_MIGRATION;
        }
    }
    return APL_OK;
}

/** Apply one migration and its version record inside a single transaction. */
static apl_status apply_migration(apl_db_connection *const connection,
                                  const apl_db_migration *const migration, apl_error *const error)
{
    sqlite3 *const handle = apl_db_handle(connection);

    apl_status status = exec_sql(handle, "BEGIN IMMEDIATE;", error);
    if (status != APL_OK) {
        return status;
    }

    status = exec_sql(handle, migration->sql, error);
    if (status == APL_OK) {
        sqlite3_stmt *record = nullptr;
        if (sqlite3_prepare_v2(
                handle,
                "INSERT INTO " SCHEMA_VERSION_TABLE
                " (version, applied_at) VALUES (?1, strftime('%Y-%m-%dT%H:%M:%fZ', 'now'));",
                -1, &record, nullptr) != SQLITE_OK) {
            status = APL_ERR_DB_QUERY;
            apl_error_set(error, APL_ERR_DB_QUERY, "cannot record migration %d: %s",
                          migration->version, sqlite3_errmsg(handle));
        } else {
            sqlite3_bind_int(record, 1, migration->version);
            if (sqlite3_step(record) != SQLITE_DONE) {
                status = APL_ERR_DB_QUERY;
                apl_error_set(error, APL_ERR_DB_QUERY, "cannot record migration %d: %s",
                              migration->version, sqlite3_errmsg(handle));
            }
            sqlite3_finalize(record);
        }
    }

    if (status != APL_OK) {
        (void)exec_sql(handle, "ROLLBACK;", error);
        if (status == APL_ERR_DB_QUERY) {
            return APL_ERR_DB_MIGRATION;
        }
        return status;
    }

    status = exec_sql(handle, "COMMIT;", error);
    if (status != APL_OK) {
        (void)exec_sql(handle, "ROLLBACK;", error);
        char context[128];
        snprintf(context, sizeof(context), "migration %d could not be committed",
                 migration->version);
        apl_error_prefix(error, APL_ERR_DB_MIGRATION, context);
        return APL_ERR_DB_MIGRATION;
    }
    return APL_OK;
}

apl_status apl_db_migrations_run_list(apl_db_connection *const connection,
                                      const apl_db_migration *const migrations, const size_t count,
                                      apl_error *const error)
{
    if (connection == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (migrations == nullptr && count > 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "migration list is nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }

    /* Validate before any database change so a bad compiled list cannot
     * modify anything. */
    apl_status status = validate_list(migrations, count, error);
    if (status != APL_OK) {
        return status;
    }

    sqlite3 *const handle = apl_db_handle(connection);
    if (handle == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "database connection is closed");
        return APL_ERR_INVALID_ARGUMENT;
    }

    status = ensure_version_table(handle, error);
    if (status != APL_OK) {
        return status;
    }

    int current_version = 0;
    status = apl_db_current_version(connection, &current_version, error);
    if (status != APL_OK) {
        return status;
    }

    const int latest_version = (count > 0) ? (int)count : 0;
    if (current_version > latest_version) {
        apl_error_set(error, APL_ERR_DB_SCHEMA_TOO_NEW,
                      "database schema version %d is newer than this application supports "
                      "(compiled migrations up to version %d); upgrade the application instead "
                      "of modifying the database",
                      current_version, latest_version);
        return APL_ERR_DB_SCHEMA_TOO_NEW;
    }

    for (size_t index = 0; index < count; ++index) {
        if (migrations[index].version <= current_version) {
            continue;
        }
        status = apply_migration(connection, &migrations[index], error);
        if (status != APL_OK) {
            char context[128];
            snprintf(context, sizeof(context), "migration to version %d failed",
                     migrations[index].version);
            apl_error_prefix(error, status, context);
            return status;
        }
    }
    return APL_OK;
}

apl_status apl_db_migrate(apl_db_connection *const connection, apl_error *const error)
{
    size_t count = 0;
    const apl_db_migration *const migrations = apl_db_migrations_list(&count);
    return apl_db_migrations_run_list(connection, migrations, count, error);
}
