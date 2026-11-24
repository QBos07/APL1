#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "db/connection.h"
#include "db/migrations.h"
#include "test_support.h"

static apl_status query_int(apl_db_connection *connection, const char *sql, int *out_value)
{
    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(apl_db_handle(connection), sql, -1, &statement, NULL) != SQLITE_OK) {
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

Test(db_migration_rollback, failing_migration_leaves_no_partial_schema_or_version_record)
{
    char dir[128];
    char path[192];

    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_temp_dir(dir, sizeof(dir), &err), APL_OK);
    snprintf(path, sizeof(path), "%s/data.sqlite3", dir);

    apl_db_connection connection = {0};
    cr_assert_eq(apl_db_open(&connection, path, &err), APL_OK);

    /* Migration 1 succeeds and commits. Migration 2 makes one valid schema
     * change and then deliberately fails, proving that its transaction rolls
     * back instead of leaving a half-applied schema. */
    const apl_db_migration failing_list[] = {
        {1, "CREATE TABLE apl_roll_probe (id INTEGER PRIMARY KEY);"},
        {2, "CREATE TABLE apl_roll_second (id INTEGER PRIMARY KEY);\n"
            "INSERT INTO apl_roll_missing VALUES (1);"},
    };

    cr_assert_eq(apl_db_migrations_run_list(&connection, failing_list, 2, &err),
                 APL_ERR_DB_MIGRATION);
    cr_assert_not(apl_error_is_success(&err));
    cr_assert_neq(strstr(apl_error_message(&err), "apl_roll_missing"), (char *)NULL);

    /* The earlier migration is still applied and recorded. */
    int version = -1;
    cr_assert_eq(apl_db_current_version(&connection, &version, NULL), APL_OK);
    cr_assert_eq(version, 1);

    int version_rows = -1;
    cr_assert_eq(query_int(&connection, "SELECT COUNT(*) FROM apl_schema_migrations;",
                           &version_rows), APL_OK);
    cr_assert_eq(version_rows, 1);

    /* The failed migration's schema change is gone and its version is not recorded. */
    int partial_tables = -1;
    cr_assert_eq(query_int(&connection,
                           "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table'"
                           " AND name = 'apl_roll_second';",
                           &partial_tables), APL_OK);
    cr_assert_eq(partial_tables, 0);

    int recorded_second = -1;
    cr_assert_eq(query_int(&connection,
                           "SELECT COUNT(*) FROM apl_schema_migrations WHERE version = 2;",
                           &recorded_second), APL_OK);
    cr_assert_eq(recorded_second, 0);

    apl_db_close(&connection);
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}

Test(db_migration_rollback, corrected_migration_recovers_on_the_same_database)
{
    char dir[128];
    char path[192];

    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_temp_dir(dir, sizeof(dir), &err), APL_OK);
    snprintf(path, sizeof(path), "%s/data.sqlite3", dir);

    apl_db_connection connection = {0};
    cr_assert_eq(apl_db_open(&connection, path, &err), APL_OK);

    const apl_db_migration failing_list[] = {
        {1, "CREATE TABLE apl_roll_probe (id INTEGER PRIMARY KEY);"},
        {2, "INSERT INTO apl_roll_missing VALUES (1);"},
    };
    cr_assert_eq(apl_db_migrations_run_list(&connection, failing_list, 2, &err),
                 APL_ERR_DB_MIGRATION);

    /* Recovery contract: a failed migration is not recorded as applied, so a
     * corrected version 2 runs against the same database and completes normally. */
    const apl_db_migration corrected_list[] = {
        {1, "CREATE TABLE apl_roll_probe (id INTEGER PRIMARY KEY);"},
        {2, "CREATE TABLE apl_roll_second (id INTEGER PRIMARY KEY);"},
    };
    apl_error_init(&err);
    cr_assert_eq(apl_db_migrations_run_list(&connection, corrected_list, 2, &err), APL_OK);

    int version = -1;
    cr_assert_eq(apl_db_current_version(&connection, &version, NULL), APL_OK);
    cr_assert_eq(version, 2);

    int version_rows = -1;
    cr_assert_eq(query_int(&connection, "SELECT COUNT(*) FROM apl_schema_migrations;",
                           &version_rows), APL_OK);
    cr_assert_eq(version_rows, 2);

    int second_tables = -1;
    cr_assert_eq(query_int(&connection,
                           "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table'"
                           " AND name = 'apl_roll_second';",
                           &second_tables), APL_OK);
    cr_assert_eq(second_tables, 1);

    apl_db_close(&connection);
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}
