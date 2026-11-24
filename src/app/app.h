#ifndef APL_APP_APP_H
#define APL_APP_APP_H

#include <stdbool.h>

#include "core/error.h"
#include "db/connection.h"

/*
 * Application lifecycle (work item 1.10).
 *
 * apl_app owns the non-interactive side of startup: database path
 * resolution, opening the SQLite connection, and running migrations, in
 * that documented order. It deliberately knows nothing about Notcurses or
 * the terminal, so the whole startup/shutdown can be exercised by tests
 * with a temporary database and no terminal input. The TUI lifecycle is
 * tested separately (see tests/tui and the 1.8/1.9 work items).
 *
 * Failure contract: a failure at any initialization step closes every
 * resource opened so far (a partially started app is never left behind),
 * and the error is returned to the caller. There is no fallback that
 * silently switches to a different database after an error.
 */

/** Explicit application configuration. Zero-initialize for production
 *  defaults (database path resolved per the 1.3 policy from the
 *  environment). Tests pass a temporary database path. */
typedef struct apl_app_config {
    /** Database path to use directly, or NULL for the environment-derived
     *  default. An explicit path must already have a usable parent
     *  directory (see src/db/path.h); it is never created implicitly. */
    const char *database_path;
} apl_app_config;

/** Owns the initialized application state. Zero-initialize before
 *  apl_app_start; apl_app_stop is safe on an uninitialized/already-stopped
 *  value and releases every owned resource exactly once. */
typedef struct apl_app {
    apl_db_connection db;
    bool db_open;
} apl_app;

/** Initialize the application: resolve the database path, open the SQLite
 *  connection, and apply migrations (the documented order).
 *
 * On success the app owns an open database connection and must be
 * released with apl_app_stop. On failure `error` (optional) receives the
 * failing step's status and message, every resource opened so far is
 * closed, and the value stays uninitialized. */
apl_status apl_app_start(apl_app *app, const apl_app_config *config, apl_error *error);

/** Release the application state: close the database connection.
 *
 * Safe on an uninitialized/zero-initialized value and on an already-stopped
 * value; each successful stop runs exactly once. */
void apl_app_stop(apl_app *app);

#endif /* APL_APP_APP_H */
