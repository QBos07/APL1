#ifndef APL_DB_DRIVER_REPOSITORY_H
#define APL_DB_DRIVER_REPOSITORY_H

#include <stddef.h>
#include <stdint.h>

#include "core/error.h"
#include "db/connection.h"
#include "domain/reference.h"

/* Shared driver identity persistence (work item 3.2). Drivers are not owned by
 * a team; team affiliation lives in dated membership rows. Duplicate display
 * names are allowed and remain separate stable IDs. */

apl_status apl_driver_repository_create(apl_db_connection *connection, const char *name,
                                        const char *short_name, const char *notes,
                                        apl_driver *out_driver, apl_error *error);

apl_status apl_driver_repository_get(apl_db_connection *connection, int64_t id,
                                     apl_driver *out_driver, apl_error *error);

apl_status apl_driver_repository_update(apl_db_connection *connection, int64_t id, const char *name,
                                        const char *short_name, const char *notes,
                                        apl_driver *out_driver, apl_error *error);

apl_status apl_driver_repository_archive(apl_db_connection *connection, int64_t id,
                                         apl_driver *out_driver, apl_error *error);

/** List the shared driver directory (optionally including archived profiles). */
apl_status apl_driver_repository_list(apl_db_connection *connection, bool include_archived,
                                      apl_driver **out_drivers, size_t *out_count,
                                      apl_error *error);

/** List drivers with at least one membership for the team, including historical
 *  periods. Team scope is required for the default People view. */
apl_status apl_driver_repository_list_for_team(apl_db_connection *connection, int64_t team_id,
                                               bool include_archived, apl_driver **out_drivers,
                                               size_t *out_count, apl_error *error);

void apl_driver_repository_free_list(apl_driver *drivers);

#endif /* APL_DB_DRIVER_REPOSITORY_H */
