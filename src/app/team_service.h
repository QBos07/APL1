#ifndef APL_APP_TEAM_SERVICE_H
#define APL_APP_TEAM_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include "app/app.h"
#include "core/error.h"
#include "domain/team.h"

/*
 * Team use cases (work items 2.7/2.8/2.9).
 *
 * Menus, commands, and tests call these functions rather than repositories
 * directly. Name/notes validation runs before SQL; multi-write operations own
 * one transaction. No TUI or SQLite statement type appears in this interface.
 */

/** How the persisted active-team setting maps to a team. */
typedef enum apl_active_team_status {
    APL_ACTIVE_TEAM_NONE = 0, /**< No selection, or the stored team no longer exists. */
    APL_ACTIVE_TEAM_ACTIVE,   /**< Selected team exists and is active. */
    APL_ACTIVE_TEAM_ARCHIVED, /**< Selected team exists but is archived; choose another. */
} apl_active_team_status;

/** Normalize, insert, and activate a new team atomically.
 *
 * Blank names and duplicates are rejected. On success the new team is the
 * active team and `out_team` holds the stored row. */
apl_status apl_team_service_create(apl_app *app, const char *name, const char *notes,
                                   apl_team *out_team, apl_error *error);

/** List teams through the application boundary.
 *
 * Release the returned array with apl_team_service_free_list. */
apl_status apl_team_service_list(apl_app *app, bool include_archived, apl_team **out_teams,
                                 size_t *out_count, apl_error *error);

/** Fetch one team by ID. */
apl_status apl_team_service_get(apl_app *app, int64_t id, apl_team *out_team, apl_error *error);

/** Validate and update a team's name/notes while preserving its ID and archive
 *  state. */
apl_status apl_team_service_update(apl_app *app, int64_t id, const char *name, const char *notes,
                                   apl_team *out_team, apl_error *error);

/** Archive a team. If it was active, clear the active-team setting in the same
 *  transaction; the archived rows/history remain. */
apl_status apl_team_service_archive(apl_app *app, int64_t id, apl_team *out_team, apl_error *error);

/** Load the persisted active-team selection.
 *
 * A missing target is reported as APL_ACTIVE_TEAM_NONE (the FK prevents this
 * in normal operation); an archived target is reported explicitly as
 * APL_ACTIVE_TEAM_ARCHIVED with the team row for context. */
apl_status apl_team_service_active(apl_app *app, apl_team *out_team,
                                   apl_active_team_status *out_status, apl_error *error);

/** Select an existing, non-archived team as active. */
apl_status apl_team_service_set_active(apl_app *app, int64_t id, apl_error *error);

/** Release a list returned by apl_team_service_list. */
void apl_team_service_free_list(apl_team *teams);

#endif /* APL_APP_TEAM_SERVICE_H */
