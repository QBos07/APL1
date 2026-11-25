#ifndef APL_DB_TEAM_REPOSITORY_H
#define APL_DB_TEAM_REPOSITORY_H

#include <stddef.h>
#include <stdint.h>

#include "core/error.h"
#include "db/connection.h"
#include "domain/team.h"

/*
 * Team persistence (work items 2.5/2.6/2.8).
 *
 * Every call borrows the connection and owns/finalizes its statements. These
 * are single-operation primitives: the application use case owns transaction
 * boundaries for multi-write operations. Names are expected to be normalized
 * by the use case; the repository still maps the database's case-insensitive
 * unique-name constraint to APL_ERR_CONFLICT.
 */

/** Insert one team and return the stored row.
 *
 * `name` must be normalized and non-empty; `notes` may be an empty string.
 * A duplicate name (case-insensitive) returns APL_ERR_CONFLICT. */
apl_status apl_team_repository_create(const apl_db_connection *connection, const char *name,
                                      const char *notes, apl_team *out_team, apl_error *error);

/** Fetch one team by stable ID; missing IDs return APL_ERR_NOT_FOUND. */
apl_status apl_team_repository_get(const apl_db_connection *connection, int64_t id,
                                   apl_team *out_team, apl_error *error);

/** Rename/update the profile text of one team and return the stored row.
 *
 * The ID and archive state are preserved. A duplicate name returns
 * APL_ERR_CONFLICT; a missing ID returns APL_ERR_NOT_FOUND. */
apl_status apl_team_repository_update(const apl_db_connection *connection, int64_t id,
                                      const char *name, const char *notes, apl_team *out_team,
                                      apl_error *error);

/** Archive one team and return the stored row.
 *
 * Repeated archive calls are allowed and preserve the original archived_at
 * value. Missing IDs return APL_ERR_NOT_FOUND. */
apl_status apl_team_repository_archive(const apl_db_connection *connection, int64_t id,
                                       apl_team *out_team, apl_error *error);

/** List teams ordered by name, with active teams before archived teams.
 *
 * When `include_archived` is false only non-archived teams are returned. On
 * success `*out_teams` points to an allocated array (nullptr when the count is
 * zero) and the caller releases it with apl_team_repository_free_list. */
apl_status apl_team_repository_list(const apl_db_connection *connection, bool include_archived,
                                    apl_team **out_teams, size_t *out_count, apl_error *error);

/** Release a list returned by apl_team_repository_list (nullptr is a no-op). */
void apl_team_repository_free_list(apl_team *teams);

#endif /* APL_DB_TEAM_REPOSITORY_H */
