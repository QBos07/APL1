#ifndef APL_DB_MIGRATIONS_H
#define APL_DB_MIGRATIONS_H

#include <stddef.h>

#include "core/error.h"
#include "db/connection.h"

/*
 * Schema versioning and migration runner.
 *
 * v1 representation decision: migrations are compiled into the application as
 * an ordered array of {version, SQL} entries; nothing is loaded from the
 * current working directory. The schema-version table is
 *
 *   apl_schema_migrations (version INTEGER PRIMARY KEY, applied_at TEXT NOT NULL)
 *
 * with one row per applied migration. The current schema version is
 * MAX(version) in that table, or 0 when it is absent/empty.
 *
 * Runner contract:
 * - The compiled list is validated before any database change: versions must
 *   be exactly 1..count in ascending order; duplicates and gaps are a
 *   programming error reported as APL_ERR_DB_MIGRATION.
 * - A database with a schema version newer than the compiled list is never
 *   modified and yields APL_ERR_DB_SCHEMA_TOO_NEW.
 * - Each migration and its version record are applied atomically in one
 *   transaction; a failed migration leaves no partial schema change and is
 *   not recorded as applied, so a corrected version can be re-run later.
 */

/** One compiled migration: `version` (1-based, contiguous) and its SQL. */
typedef struct apl_db_migration {
    int version;
    const char *sql;
} apl_db_migration;

/** Apply the compiled-in production migrations (apl_db_migrations list). */
apl_status apl_db_migrate(apl_db_connection *connection, apl_error *error);

/** Run a caller-supplied migration list. Production callers use
 *  apl_db_migrate; tests use this entry point to exercise the runner with
 *  their own (including deliberately failing) lists. */
apl_status apl_db_migrations_run_list(apl_db_connection *connection,
                                      const apl_db_migration *migrations,
                                      size_t count,
                                      apl_error *error);

/** Report the current schema version (0 for a fresh, unmigrated database). */
apl_status apl_db_current_version(apl_db_connection *connection, int *out_version,
                                  apl_error *error);

#endif /* APL_DB_MIGRATIONS_H */
