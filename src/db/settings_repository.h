#ifndef APL_DB_SETTINGS_REPOSITORY_H
#define APL_DB_SETTINGS_REPOSITORY_H

#include <stdint.h>

#include "core/error.h"
#include "db/connection.h"

/*
 * Persisted application settings (work item 2.9).
 *
 * The singleton app_settings row stores only the active team ID. A value of 0
 * means no team is selected. Missing/archived validation belongs to the use
 * case; this repository only persists the value and can clear it atomically
 * with an archive use case.
 */

/** Read the stored active team ID; 0 means no selection. */
apl_status apl_settings_repository_get_active_team_id(const apl_db_connection *connection,
                                                      int64_t *out_team_id, apl_error *error);

/** Set the active team to an existing, non-archived team.
 *
 * A missing or archived team returns APL_ERR_NOT_FOUND and leaves the previous
 * selection unchanged. */
apl_status apl_settings_repository_set_active_team_id(const apl_db_connection *connection,
                                                      int64_t team_id, apl_error *error);

/** Clear the active-team selection (safe when already empty). */
apl_status apl_settings_repository_clear_active_team_id(const apl_db_connection *connection,
                                                        apl_error *error);

#endif /* APL_DB_SETTINGS_REPOSITORY_H */
