#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "db/connection.h"
#include "test_support.h"

/** Open one temporary database and return its connection. */
static apl_status open_temp_database(apl_db_connection *connection, char *dir, size_t dir_size,
                                     char *path, size_t path_size)
{
    apl_error err;
    apl_error_init(&err);
    apl_status status = apl_test_temp_dir(dir, dir_size, &err);
    if (status != APL_OK) {
        return status;
    }
    const int written = snprintf(path, path_size, "%s/data.sqlite3", dir);
    if (written < 0) {
        return APL_ERR_IO;
    }
    return apl_db_open(connection, path, &err);
}

Test(db_foreign_keys, new_connection_reports_enforcement_enabled)
{
    char dir[128];
    char path[192];
    apl_error err;
    apl_error_init(&err);

    apl_db_connection connection = {0};
    cr_assert_eq(open_temp_database(&connection, dir, sizeof(dir), path, sizeof(path)), APL_OK);

    sqlite3_stmt *pragma = NULL;
    cr_assert_eq(sqlite3_prepare_v2(apl_db_handle(&connection), "PRAGMA foreign_keys;", -1,
                                    &pragma, NULL), SQLITE_OK);
    cr_assert_eq(sqlite3_step(pragma), SQLITE_ROW);
    cr_assert_eq(sqlite3_column_int(pragma, 0), 1);
    sqlite3_finalize(pragma);

    apl_db_close(&connection);
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}

Test(db_foreign_keys, invalid_child_insert_is_rejected)
{
    char dir[128];
    char path[192];
    apl_error err;
    apl_error_init(&err);

    apl_db_connection connection = {0};
    cr_assert_eq(open_temp_database(&connection, dir, sizeof(dir), path, sizeof(path)), APL_OK);
    sqlite3 *handle = apl_db_handle(&connection);

    char *sqlite_error = NULL;
    cr_assert_eq(sqlite3_exec(handle,
                              "CREATE TABLE teams (id INTEGER PRIMARY KEY, name TEXT NOT NULL);"
                              "CREATE TABLE members (id INTEGER PRIMARY KEY,"
                              " team_id INTEGER NOT NULL REFERENCES teams(id));",
                              NULL, NULL, &sqlite_error), SQLITE_OK);

    /* A child row without a matching parent must fail at the SQLite API level. */
    cr_assert_eq(sqlite3_exec(handle, "INSERT INTO members (team_id) VALUES (999);",
                              NULL, NULL, &sqlite_error), SQLITE_CONSTRAINT);
    sqlite3_free(sqlite_error);
    sqlite_error = NULL;

    /* Once the parent exists, the child insert succeeds. */
    cr_assert_eq(sqlite3_exec(handle, "INSERT INTO teams (name) VALUES ('probe');"
                                      "INSERT INTO members (team_id) VALUES (1);",
                               NULL, NULL, &sqlite_error), SQLITE_OK);
    sqlite3_free(sqlite_error);

    apl_db_close(&connection);
    /* Removing the directory removes the test schema and data with it. */
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}
