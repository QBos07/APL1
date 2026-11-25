#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <criterion/criterion.h>

#include "db/connection.h"
#include "test_support.h"

/** Callback that stores the first column of the first returned row as int. */
static int first_int_callback(void *const out, const int argc, char **argv,
                              [[maybe_unused]] char **column_names)
{
    int *const value = out;
    if (argc > 0 && argv[0] != nullptr) {
        *value = atoi(argv[0]);
    }
    return 0;
}

/** Create the temporary directory and database path used by one test. */
static apl_status make_test_database(char *dir, const size_t dir_size, char *path,
                                     const size_t path_size)
{
    apl_error err;
    apl_error_init(&err);
    const apl_status status = apl_test_temp_dir(dir, dir_size, &err);
    if (status != APL_OK) {
        return status;
    }
    const int written = snprintf(path, path_size, "%s/data.sqlite3", dir);
    if (written < 0) {
        return APL_ERR_IO;
    }
    return APL_OK;
}

Test(db_connection, open_creates_usable_database)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    cr_assert_eq(make_test_database(dir, sizeof(dir), path, sizeof(path)), APL_OK);

    apl_error err;
    apl_error_init(&err);

    apl_db_connection connection = {};
    cr_assert_eq(apl_db_open(&connection, path, &err), APL_OK);
    cr_assert_not_null(apl_db_handle(&connection));

    char *sqlite_error = nullptr;
    sqlite3 *const handle = apl_db_handle(&connection);
    cr_assert_eq(sqlite3_exec(handle, "CREATE TABLE t (x INTEGER); INSERT INTO t (x) VALUES (42);",
                              nullptr, nullptr, &sqlite_error),
                 SQLITE_OK);
    int value = -1;
    cr_assert_eq(
        sqlite3_exec(handle, "SELECT x FROM t;", first_int_callback, &value, &sqlite_error),
        SQLITE_OK);
    cr_assert_eq(value, 42);

    apl_db_close(&connection);
    cr_assert_null(apl_db_handle(&connection));

    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}

Test(db_connection, reopen_persists_data)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    cr_assert_eq(make_test_database(dir, sizeof(dir), path, sizeof(path)), APL_OK);

    apl_error err;
    apl_error_init(&err);

    apl_db_connection connection = {};
    cr_assert_eq(apl_db_open(&connection, path, &err), APL_OK);
    char *sqlite_error = nullptr;
    cr_assert_eq(sqlite3_exec(apl_db_handle(&connection),
                              "CREATE TABLE t (x INTEGER); INSERT INTO t (x) VALUES (7);", nullptr,
                              nullptr, &sqlite_error),
                 SQLITE_OK);
    apl_db_close(&connection);

    cr_assert_eq(apl_db_open(&connection, path, &err), APL_OK);
    int value = -1;
    cr_assert_eq(sqlite3_exec(apl_db_handle(&connection), "SELECT x FROM t;", first_int_callback,
                              &value, &sqlite_error),
                 SQLITE_OK);
    cr_assert_eq(value, 7);

    apl_db_close(&connection);
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}

Test(db_connection, close_is_safe_on_uninitialized_and_repeated)
{
    apl_db_connection uninitialized = {};
    apl_db_close(&uninitialized); /* must not crash */
    apl_db_close(&uninitialized); /* already closed: documented no-op */

    cr_assert_null(apl_db_handle(&uninitialized));
}

Test(db_connection, invalid_path_fails_without_leaked_handle)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    cr_assert_eq(apl_test_temp_dir(dir, sizeof(dir), &err), APL_OK);

    /* The parent directory exists but the 'missing-dir' component does not. */
    snprintf(path, sizeof(path), "%s/missing-dir/data.sqlite3", dir);

    apl_db_connection connection = {};
    cr_assert_eq(apl_db_open(&connection, path, &err), APL_ERR_DB_OPEN);
    cr_assert_null(apl_db_handle(&connection));

    cr_assert_not(apl_error_is_success(&err));
    cr_assert_neq(strstr(apl_error_message(&err), "cannot open database"), nullptr);

    apl_db_close(&connection); /* must be safe after a failed open */
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}

Test(db_connection, null_or_empty_path_is_rejected)
{
    apl_db_connection connection = {};

    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_db_open(&connection, nullptr, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_db_open(&connection, "", &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_null(apl_db_handle(&connection));
    cr_assert_eq(apl_error_code(&err), APL_ERR_INVALID_ARGUMENT);
}
