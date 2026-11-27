#include <stdio.h>

#include <criterion/criterion.h>

#include "db/connection.h"
#include "db/migrations.h"
#include "test_support.h"

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

/** Open one temporary database. */
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

Test(db_initial_migration, new_database_is_marked_at_latest_version)
{
    char dir[APL_TEST_DIR_MAX];
    apl_db_connection connection = {};
    cr_assert_eq(open_temp_database(&connection, dir, sizeof(dir)), APL_OK);

    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_db_migrate(&connection, &err), APL_OK);

    int version = -1;
    cr_assert_eq(apl_db_current_version(&connection, &version, nullptr), APL_OK);
    cr_assert_eq(version, 21);

    int version_rows = -1;
    cr_assert_eq(
        query_int(&connection, "SELECT COUNT(*) FROM apl_schema_migrations;", &version_rows),
        APL_OK);
    cr_assert_eq(version_rows, 21);

    apl_db_close(&connection);
    apl_error_init(&err);
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}

Test(db_initial_migration, baseline_contains_no_team_or_sample_records)
{
    char dir[APL_TEST_DIR_MAX];
    apl_db_connection connection = {};
    cr_assert_eq(open_temp_database(&connection, dir, sizeof(dir)), APL_OK);

    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_db_migrate(&connection, &err), APL_OK);

    /* Migration metadata, team/settings, reference, and audit tables exist. */
    int table_count = -1;
    cr_assert_eq(query_int(&connection, "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table';",
                           &table_count),
                 APL_OK);
    cr_assert_eq(table_count, 18);

    /* No team or sample domain record is seeded. */
    int team_rows = -1;
    cr_assert_eq(query_int(&connection, "SELECT COUNT(*) FROM teams;", &team_rows), APL_OK);
    cr_assert_eq(team_rows, 0);

    int settings_rows = -1;
    cr_assert_eq(query_int(&connection, "SELECT COUNT(*) FROM app_settings;", &settings_rows),
                 APL_OK);
    cr_assert_eq(settings_rows, 1);

    int active_rows = -1;
    cr_assert_eq(query_int(&connection,
                           "SELECT COUNT(*) FROM app_settings WHERE active_team_id IS NULL;",
                           &active_rows),
                 APL_OK);
    cr_assert_eq(active_rows, 1);

    apl_db_close(&connection);
    apl_error_init(&err);
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}

Test(db_initial_migration, second_migration_run_is_a_noop)
{
    char dir[APL_TEST_DIR_MAX];
    apl_db_connection connection = {};
    cr_assert_eq(open_temp_database(&connection, dir, sizeof(dir)), APL_OK);

    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_db_migrate(&connection, &err), APL_OK);

    int version_rows_before = -1;
    cr_assert_eq(
        query_int(&connection, "SELECT COUNT(*) FROM apl_schema_migrations;", &version_rows_before),
        APL_OK);

    cr_assert_eq(apl_db_migrate(&connection, &err), APL_OK);

    int version = -1;
    cr_assert_eq(apl_db_current_version(&connection, &version, nullptr), APL_OK);
    cr_assert_eq(version, 21);

    int version_rows_after = -1;
    cr_assert_eq(
        query_int(&connection, "SELECT COUNT(*) FROM apl_schema_migrations;", &version_rows_after),
        APL_OK);
    cr_assert_eq(version_rows_after, version_rows_before);

    apl_db_close(&connection);
    apl_error_init(&err);
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}

Test(db_initial_migration, version_one_database_upgrades_through_team_and_settings_migrations)
{
    char dir[APL_TEST_DIR_MAX];
    apl_db_connection connection = {};
    cr_assert_eq(open_temp_database(&connection, dir, sizeof(dir)), APL_OK);

    apl_error err;
    apl_error_init(&err);

    /* Simulate a database created by the Phase 1 application at version 1. */
    size_t production_count = 0;
    const apl_db_migration *const production = apl_db_migrations_list(&production_count);
    cr_assert_eq(production_count, 21);
    cr_assert_eq(apl_db_migrations_run_list(&connection, production, 1, &err), APL_OK);

    int version = -1;
    cr_assert_eq(apl_db_current_version(&connection, &version, nullptr), APL_OK);
    cr_assert_eq(version, 1);

    /* Startup migrates it forward exactly once. */
    cr_assert_eq(apl_db_migrate(&connection, &err), APL_OK);
    cr_assert_eq(apl_db_current_version(&connection, &version, nullptr), APL_OK);
    cr_assert_eq(version, 21);

    int team_rows = -1;
    cr_assert_eq(query_int(&connection, "SELECT COUNT(*) FROM teams;", &team_rows), APL_OK);
    cr_assert_eq(team_rows, 0);

    int settings_rows = -1;
    cr_assert_eq(query_int(&connection, "SELECT COUNT(*) FROM app_settings;", &settings_rows),
                 APL_OK);
    cr_assert_eq(settings_rows, 1);

    apl_db_close(&connection);
    apl_error_init(&err);
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}
