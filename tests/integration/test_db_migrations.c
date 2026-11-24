#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "db/connection.h"
#include "db/migrations.h"
#include "test_support.h"

/** Open a temporary database for one test. */
static apl_status open_temp_database(apl_db_connection *connection, char *dir,
                                     const size_t dir_size)
{
    char path[APL_TEST_PATH_MAX];

    apl_error err;
    apl_error_init(&err);
    const apl_status status = apl_test_temp_dir(dir, dir_size, &err);
    if (status != APL_OK) {
        return status;
    }
    snprintf(path, sizeof(path), "%s/data.sqlite3", dir);
    return apl_db_open(connection, path, &err);
}

/** Query a single integer through the connection. */
static apl_status query_int(const apl_db_connection *const connection, const char *const sql,
                            int *const out_value)
{
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(apl_db_handle(connection), sql, -1, &statement, nullptr) != SQLITE_OK) {
        return APL_ERR_DB_QUERY;
    }
    if (sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement);
        return APL_ERR_DB_QUERY;
    }
    *out_value = sqlite3_column_int(statement, 0);
    sqlite3_finalize(statement);
    return APL_OK;
}

/** Run a query whose result must equal `expected`. */
static void expect_int(const apl_db_connection *const connection, const char *const sql,
                       const int expected)
{
    int value = -1;
    cr_expect_eq(query_int(connection, sql, &value), APL_OK);
    cr_assert_eq(value, expected, "query '%s' returned %d instead of %d", sql, value, expected);
}

Test(db_migrations, first_run_creates_version_table_and_reaches_latest)
{
    char dir[APL_TEST_DIR_MAX];
    apl_db_connection connection = {};
    cr_assert_eq(open_temp_database(&connection, dir, sizeof(dir)), APL_OK);

    int version = -1;
    cr_assert_eq(apl_db_current_version(&connection, &version, nullptr), APL_OK);
    cr_assert_eq(version, 0);

    const apl_db_migration list[] = {
        {1, "CREATE TABLE probe_one (id INTEGER PRIMARY KEY);"},
        {2, "CREATE TABLE probe_two (id INTEGER PRIMARY KEY);"},
    };

    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_db_migrations_run_list(&connection, list, 2, &err), APL_OK);

    cr_assert_eq(apl_db_current_version(&connection, &version, nullptr), APL_OK);
    cr_assert_eq(version, 2);
    expect_int(&connection, "SELECT COUNT(*) FROM apl_schema_migrations;", 2);
    expect_int(&connection,
               "SELECT COUNT(*) FROM apl_schema_migrations WHERE applied_at IS NOT NULL;", 2);
    expect_int(&connection,
               "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name LIKE 'probe%';",
               2);

    apl_db_close(&connection);
    apl_error_init(&err);
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}

Test(db_migrations, repeated_run_is_a_noop)
{
    char dir[APL_TEST_DIR_MAX];
    apl_db_connection connection = {};
    cr_assert_eq(open_temp_database(&connection, dir, sizeof(dir)), APL_OK);

    const apl_db_migration list[] = {
        {1, "CREATE TABLE probe_one (id INTEGER PRIMARY KEY);"},
    };

    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_db_migrations_run_list(&connection, list, 1, &err), APL_OK);

    int tables_before = 0;
    cr_assert_eq(query_int(&connection, "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table';",
                           &tables_before),
                 APL_OK);

    /* Second run: schema version, schema, and records must be unchanged. */
    cr_assert_eq(apl_db_migrations_run_list(&connection, list, 1, &err), APL_OK);
    int version = -1;
    cr_assert_eq(apl_db_current_version(&connection, &version, nullptr), APL_OK);
    cr_assert_eq(version, 1);
    expect_int(&connection, "SELECT COUNT(*) FROM apl_schema_migrations;", 1);

    int tables_after = 0;
    cr_assert_eq(query_int(&connection, "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table';",
                           &tables_after),
                 APL_OK);
    cr_assert_eq(tables_after, tables_before);

    apl_db_close(&connection);
    apl_error_init(&err);
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}

Test(db_migrations, newer_database_is_reported_and_not_modified)
{
    char dir[APL_TEST_DIR_MAX];
    apl_db_connection connection = {};
    cr_assert_eq(open_temp_database(&connection, dir, sizeof(dir)), APL_OK);

    const apl_db_migration list[] = {
        {1, "CREATE TABLE probe_one (id INTEGER PRIMARY KEY);"},
    };

    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_db_migrations_run_list(&connection, list, 1, &err), APL_OK);

    /* Simulate a database written by a newer application version. */
    char *sqlite_error = nullptr;
    cr_assert_eq(sqlite3_exec(apl_db_handle(&connection),
                              "INSERT INTO apl_schema_migrations (version, applied_at)"
                              " VALUES (99, '2030-01-01T00:00:00.000Z');",
                              nullptr, nullptr, &sqlite_error),
                 SQLITE_OK);

    cr_assert_eq(apl_db_migrations_run_list(&connection, list, 1, &err), APL_ERR_DB_SCHEMA_TOO_NEW);
    cr_assert_neq(strstr(apl_error_message(&err), "newer than this application supports"), nullptr);

    /* Only the two pre-existing version rows exist; nothing else was touched. */
    expect_int(&connection, "SELECT COUNT(*) FROM apl_schema_migrations;", 2);
    expect_int(&connection, "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table';", 2);

    apl_db_close(&connection);
    apl_error_init(&err);
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}

Test(db_migrations, duplicate_versions_are_rejected_without_touching_database)
{
    char dir[APL_TEST_DIR_MAX];
    apl_db_connection connection = {};
    cr_assert_eq(open_temp_database(&connection, dir, sizeof(dir)), APL_OK);

    const apl_db_migration list[] = {
        {1, "CREATE TABLE probe_one (id INTEGER PRIMARY KEY);"},
        {1, "CREATE TABLE probe_again (id INTEGER PRIMARY KEY);"},
    };

    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_db_migrations_run_list(&connection, list, 2, &err), APL_ERR_DB_MIGRATION);
    cr_assert_neq(strstr(apl_error_message(&err), "invalid"), nullptr);

    /* The version table must not even exist: validation happened first. */
    expect_int(&connection, "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table';", 0);

    apl_db_close(&connection);
    apl_error_init(&err);
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}

Test(db_migrations, gaps_are_rejected_without_touching_database)
{
    char dir[APL_TEST_DIR_MAX];
    apl_db_connection connection = {};
    cr_assert_eq(open_temp_database(&connection, dir, sizeof(dir)), APL_OK);

    const apl_db_migration list[] = {
        {1, "CREATE TABLE probe_one (id INTEGER PRIMARY KEY);"},
        {3, "CREATE TABLE probe_three (id INTEGER PRIMARY KEY);"},
    };

    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_db_migrations_run_list(&connection, list, 2, &err), APL_ERR_DB_MIGRATION);

    expect_int(&connection, "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table';", 0);

    apl_db_close(&connection);
    apl_error_init(&err);
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}

Test(db_migrations, current_version_is_zero_for_fresh_database)
{
    char dir[APL_TEST_DIR_MAX];
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_temp_dir(dir, sizeof(dir), &err), APL_OK);

    char path[APL_TEST_PATH_MAX];
    snprintf(path, sizeof(path), "%s/data.sqlite3", dir);

    apl_db_connection connection = {};
    cr_assert_eq(apl_db_open(&connection, path, &err), APL_OK);

    int version = -1;
    cr_assert_eq(apl_db_current_version(&connection, &version, nullptr), APL_OK);
    cr_assert_eq(version, 0);

    apl_db_close(&connection);
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}
