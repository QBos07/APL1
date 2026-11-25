#include <sqlite3.h>
#include <stdio.h>
#include <unistd.h>

#include <criterion/criterion.h>

#include "app/app.h"
#include "db/migrations.h"
#include "test_support.h"

/* Application lifecycle tests (1.10): startup/shutdown exercised with a
 * temporary SQLite database and no terminal input. The TUI lifecycle is
 * tested separately in tests/tui and smoke-checked in the 1.9 work item. */

/** Open a fresh temporary database, returning the directory and file path. */
static apl_status make_temp_database(char *dir, const size_t dir_size, char *path,
                                     const size_t path_size)
{
    apl_error err;
    apl_error_init(&err);

    if (apl_test_temp_dir(dir, dir_size, &err) != APL_OK) {
        return APL_ERR_IO;
    }
    const int written = snprintf(path, path_size, "%s/data.sqlite3", dir);
    if (written < 0 || (size_t)written >= path_size) {
        return APL_ERR_IO;
    }
    return APL_OK;
}

Test(app_lifecycle, start_and_stop_with_temporary_database)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(make_temp_database(dir, sizeof(dir), path, sizeof(path)), APL_OK);

    const apl_app_config config = {.database_path = path};

    apl_app app = {};
    cr_assert_eq(apl_app_start(&app, &config, &err), APL_OK);
    cr_assert(apl_error_is_success(&err));
    cr_assert_not_null(apl_db_handle(&app.db));

    /* Initialization reaches the expected schema version (the 1.6 initial
     * migration) and cleanup closes the database. */
    int version = -1;
    cr_assert_eq(apl_db_current_version(&app.db, &version, &err), APL_OK);
    cr_assert_eq(version, 14);

    apl_app_stop(&app);
    cr_assert_null(apl_db_handle(&app.db));

    /* The database file survives cleanup; it is user data, not test state. */
    cr_assert_eq(access(path, F_OK), 0);

    apl_error_init(&err);
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}

Test(app_lifecycle, explicit_path_without_parent_directory_fails_without_leak)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_test_temp_dir(dir, sizeof(dir), &err), APL_OK);
    const int written = snprintf(path, sizeof(path), "%s/no-such-dir/data.sqlite3", dir);
    cr_assert(written > 0 && (size_t)written < sizeof(path));

    const apl_app_config config = {.database_path = path};

    apl_app app = {};
    cr_assert_eq(apl_app_start(&app, &config, &err), APL_ERR_DB_OPEN);
    cr_assert_not(apl_error_is_success(&err));
    cr_assert_null(apl_db_handle(&app.db)); /* nothing leaked */

    apl_app_stop(&app); /* safe after a failed start */
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}

Test(app_lifecycle, schema_too_new_database_fails_without_leak)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(make_temp_database(dir, sizeof(dir), path, sizeof(path)), APL_OK);

    /* Write a database produced by a newer application: schema version 99. */
    sqlite3 *raw = nullptr;
    cr_assert_eq(sqlite3_open(path, &raw), SQLITE_OK);
    char *errmsg = nullptr;
    cr_assert_eq(sqlite3_exec(raw,
                              "CREATE TABLE apl_schema_migrations ("
                              "version INTEGER PRIMARY KEY);"
                              "INSERT INTO apl_schema_migrations (version) VALUES (99);",
                              nullptr, nullptr, &errmsg),
                 SQLITE_OK);
    sqlite3_free(errmsg);
    cr_assert_eq(sqlite3_close(raw), SQLITE_OK);

    const apl_app_config config = {.database_path = path};

    apl_app app = {};
    cr_assert_eq(apl_app_start(&app, &config, &err), APL_ERR_DB_SCHEMA_TOO_NEW);
    cr_assert_not(apl_error_is_success(&err));
    cr_assert_null(apl_db_handle(&app.db)); /* the connection was closed on failure */

    apl_error_init(&err);
    cr_assert_eq(apl_test_remove_temp_dir(dir, &err), APL_OK);
}

Test(app_lifecycle, stop_on_uninitialized_and_null_app_is_safe)
{
    apl_app app = {};
    apl_app_stop(&app); /* must not crash */
    apl_app_stop(nullptr);
    cr_assert(true);
}
