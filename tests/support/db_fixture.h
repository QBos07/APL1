#ifndef APL_TESTS_SUPPORT_DB_FIXTURE_H
#define APL_TESTS_SUPPORT_DB_FIXTURE_H

#include "core/error.h"
#include "db/connection.h"
#include "test_support.h"

/*
 * One temporary, migrated SQLite database per test.
 *
 * The fixture owns the directory and connection. Close it through
 * apl_test_db_close so the connection is released and the temporary directory
 * is removed (a cleanup failure is reported to the test).
 */

typedef struct apl_test_db {
    char directory[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_db_connection connection;
    bool opened;
} apl_test_db;

/** Create a unique temporary directory, open `data.sqlite3`, and migrate it. */
apl_status apl_test_db_open(apl_test_db *fixture, apl_error *error);

/** Close the connection and remove the temporary directory. Safe on an
 *  uninitialized/partially opened fixture. */
apl_status apl_test_db_close(apl_test_db *fixture, apl_error *error);

#endif /* APL_TESTS_SUPPORT_DB_FIXTURE_H */
