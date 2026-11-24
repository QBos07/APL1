#include "db_fixture.h"

#include <stdio.h>

#include "db/migrations.h"

apl_status apl_test_db_open(apl_test_db *const fixture, apl_error *const error)
{
    if (fixture == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "test database fixture is nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *fixture = (apl_test_db){};

    apl_status status = apl_test_temp_dir(fixture->directory, sizeof(fixture->directory), error);
    if (status != APL_OK) {
        return status;
    }

    const int written =
        snprintf(fixture->path, sizeof(fixture->path), "%s/data.sqlite3", fixture->directory);
    if (written < 0 || (size_t)written >= sizeof(fixture->path)) {
        apl_error_set(error, APL_ERR_IO, "test database path does not fit its buffer");
        (void)apl_test_remove_temp_dir(fixture->directory, nullptr);
        *fixture = (apl_test_db){};
        return APL_ERR_IO;
    }

    status = apl_db_open(&fixture->connection, fixture->path, error);
    if (status != APL_OK) {
        (void)apl_test_remove_temp_dir(fixture->directory, nullptr);
        *fixture = (apl_test_db){};
        return status;
    }
    fixture->opened = true;

    status = apl_db_migrate(&fixture->connection, error);
    if (status != APL_OK) {
        apl_db_close(&fixture->connection);
        (void)apl_test_remove_temp_dir(fixture->directory, nullptr);
        *fixture = (apl_test_db){};
        return status;
    }
    return APL_OK;
}

apl_status apl_test_db_close(apl_test_db *const fixture, apl_error *const error)
{
    if (fixture == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "test database fixture is nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }

    apl_db_close(&fixture->connection);
    fixture->opened = false;
    if (fixture->directory[0] == '\0') {
        return APL_OK;
    }
    const apl_status status = apl_test_remove_temp_dir(fixture->directory, error);
    fixture->directory[0] = '\0';
    fixture->path[0] = '\0';
    return status;
}
