#include "app/app.h"

#include <stdio.h>

#include "db/migrations.h"
#include "db/path.h"

/** Start the database layer: resolve the path, open the connection, and
 *  migrate. Any failure is reported and leaves no open connection. */
static apl_status start_database(apl_db_connection *db, const apl_app_config *config,
                                 apl_error *error)
{
    char path[APL_DB_PATH_MAX];

    if (config->database_path != NULL && config->database_path[0] != '\0') {
        /* Explicit paths must already have a usable parent directory; no
         * implicit creation, so typos are not silently masked (1.3). */
        const int written = snprintf(path, sizeof(path), "%s", config->database_path);
        if (written < 0 || (size_t)written >= sizeof(path)) {
            apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                          "database path does not fit in %zu bytes", sizeof(path));
            return APL_ERR_INVALID_ARGUMENT;
        }
    } else {
        apl_status status = apl_db_resolve_path(NULL, path, sizeof(path), error);
        if (status != APL_OK) {
            return status;
        }
        status = apl_db_ensure_parent_directory(path, error);
        if (status != APL_OK) {
            return status;
        }
    }

    apl_status status = apl_db_open(db, path, error);
    if (status != APL_OK) {
        return status;
    }

    status = apl_db_migrate(db, error);
    if (status != APL_OK) {
        apl_db_close(db);
        return status;
    }
    return APL_OK;
}

apl_status apl_app_start(apl_app *app, const apl_app_config *config, apl_error *error)
{
    if (app == NULL || config == NULL) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "app or config is NULL");
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (app->db_open) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "app is already started");
        return APL_ERR_INVALID_ARGUMENT;
    }

    const apl_status status = start_database(&app->db, config, error);
    if (status != APL_OK) {
        /* Nothing beyond the connection was opened; the connection is
         * closed by start_database on a migration failure and was never
         * opened on a path/open failure. */
        app->db_open = false;
        return status;
    }
    app->db_open = true;
    return APL_OK;
}

void apl_app_stop(apl_app *app)
{
    if (app == NULL) {
        return;
    }
    apl_db_close(&app->db); /* documented no-op on a closed connection */
    app->db_open = false;
}
