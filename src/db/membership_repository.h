#ifndef APL_DB_MEMBERSHIP_REPOSITORY_H
#define APL_DB_MEMBERSHIP_REPOSITORY_H

#include <stddef.h>
#include <stdint.h>

#include "core/error.h"
#include "db/connection.h"
#include "domain/reference.h"

/* Dated team-driver membership persistence (work item 3.3). A membership is a
 * period; rejoining creates a new row instead of rewriting an old period. */

apl_status apl_membership_repository_create(const apl_db_connection *connection, int64_t team_id,
                                            int64_t driver_id, const char *start_date,
                                            const char *end_date, const char *role,
                                            const char *notes, apl_membership *out_membership,
                                            apl_error *error);

apl_status apl_membership_repository_get(const apl_db_connection *connection, int64_t team_id,
                                         int64_t id, apl_membership *out_membership,
                                         apl_error *error);

apl_status apl_membership_repository_update(const apl_db_connection *connection, int64_t team_id,
                                            int64_t id, const char *start_date,
                                            const char *end_date, const char *role,
                                            const char *notes, apl_membership *out_membership,
                                            apl_error *error);

/** Set the end date of one membership; the prior period row is preserved. */
apl_status apl_membership_repository_end(const apl_db_connection *connection, int64_t team_id,
                                         int64_t id, const char *end_date,
                                         apl_membership *out_membership, apl_error *error);

/** List team memberships. When include_ended is false only rows with no end date
 *  (the current roster) are returned. */
apl_status apl_membership_repository_list_for_team(const apl_db_connection *connection,
                                                   int64_t team_id, bool include_ended,
                                                   apl_membership **out_memberships,
                                                   size_t *out_count, apl_error *error);

/** List every dated period for one shared driver, oldest first. */
apl_status apl_membership_repository_list_for_driver(const apl_db_connection *connection,
                                                     int64_t driver_id,
                                                     apl_membership **out_memberships,
                                                     size_t *out_count, apl_error *error);

void apl_membership_repository_free_list(apl_membership *memberships);

#endif /* APL_DB_MEMBERSHIP_REPOSITORY_H */
