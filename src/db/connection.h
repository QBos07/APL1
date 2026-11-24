#ifndef APL_DB_CONNECTION_H
#define APL_DB_CONNECTION_H

#include <sqlite3.h>

#include "core/error.h"

/*
 * SQLite connection lifecycle for apl1.
 *
 * The connection wrapper owns exactly one sqlite3 handle. Callers open it with
 * a fully resolved path (path resolution is a separate concern, see
 * path.h) and close it through apl_db_close.
 *
 * Open flags decision (v1): SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE with the
 * default threading model. The application is a single-process local tool, so
 * no FULLMUTEX, no URI handling, and no shared-cache flags are needed.
 *
 * Database code never calls Notcurses and never prints to the terminal.
 */

/** Owns one sqlite3 handle. Treat as an opaque value: use the functions below.
 *  A connection must be zero-initialized (or previously closed) before it is
 *  passed to apl_db_open. */
typedef struct apl_db_connection {
    sqlite3 *handle;
} apl_db_connection;

/** Open (creating if necessary) the SQLite database file at `path`.
 *
 * On success the connection owns the handle and callers must eventually call
 * apl_db_close. On failure `error` (optional) receives an APL_ERR_DB_OPEN
 * result with the SQLite message captured before the partially created handle
 * is released; `connection->handle` is nullptr afterwards.
 */
apl_status apl_db_open(apl_db_connection *connection, const char *path, apl_error *error);

/** Close the connection and release the handle.
 *
 * Safe to call on an uninitialized (zeroed) connection and on an
 * already-closed connection: a nullptr handle is a documented no-op. Only call
 * while no statements are active; this module never keeps statements open.
 */
void apl_db_close(apl_db_connection *connection);

/** Borrow the underlying SQLite handle for repository code, or nullptr when the
 *  connection is closed. The connection keeps ownership. */
sqlite3 *apl_db_handle(const apl_db_connection *connection);

#endif /* APL_DB_CONNECTION_H */
