#ifndef APL_DB_PROJECT_UPDATE_REPOSITORY_H
#define APL_DB_PROJECT_UPDATE_REPOSITORY_H

#include <stddef.h>
#include <stdint.h>

#include "core/error.h"
#include "db/connection.h"
#include "domain/project.h"

/*
 * Append-only dated project updates (work item 5.3).
 *
 * The repository has no update/delete operation for a previously stored
 * timeline entry. Team scope is inherited from the parent project: every
 * append and read verifies the project belongs to the supplied team. Updates
 * remain readable after the parent project is archived.
 */

apl_status apl_project_update_repository_append(const apl_db_connection *connection,
                                                int64_t team_id, const apl_project_update *update,
                                                apl_project_update *out_update, apl_error *error);

apl_status apl_project_update_repository_get(const apl_db_connection *connection, int64_t team_id,
                                             int64_t id, apl_project_update *out_update,
                                             apl_error *error);

/** List one project's updates in entered-date order with stable ID tie-breaks. */
apl_status apl_project_update_repository_list(const apl_db_connection *connection, int64_t team_id,
                                              int64_t project_id, apl_project_update **out_updates,
                                              size_t *out_count, apl_error *error);

void apl_project_update_repository_free_list(apl_project_update *updates);

#endif /* APL_DB_PROJECT_UPDATE_REPOSITORY_H */
