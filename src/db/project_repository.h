#ifndef APL_DB_PROJECT_REPOSITORY_H
#define APL_DB_PROJECT_REPOSITORY_H

#include <stddef.h>
#include <stdint.h>

#include "core/error.h"
#include "db/connection.h"
#include "domain/project.h"

/* Team-scoped development-project persistence (work item 5.1).
 *
 * Every read requires an explicit team ID. Optional season/car links must
 * belong to the same team; the schema enforces that relationship and the
 * repository maps violations to validation errors. Archived rows stay
 * readable through get/list with explicit archived inclusion.
 */

apl_status apl_project_repository_create(const apl_db_connection *connection,
                                         const apl_project *project, apl_project *out_project,
                                         apl_error *error);

apl_status apl_project_repository_get(const apl_db_connection *connection, int64_t team_id,
                                      int64_t id, apl_project *out_project, apl_error *error);

/** Replace the mutable fields of an existing project in the same team. */
apl_status apl_project_repository_update(const apl_db_connection *connection,
                                         const apl_project *project, apl_project *out_project,
                                         apl_error *error);

apl_status apl_project_repository_archive(const apl_db_connection *connection, int64_t team_id,
                                          int64_t id, apl_project *out_project, apl_error *error);

/** List projects ordered by title (case-insensitive) then stable ID. */
apl_status apl_project_repository_list(const apl_db_connection *connection,
                                       const apl_project_filter *filter, apl_project **out_projects,
                                       size_t *out_count, apl_error *error);

void apl_project_repository_free_list(apl_project *projects);

#endif /* APL_DB_PROJECT_REPOSITORY_H */
