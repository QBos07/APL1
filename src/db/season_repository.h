#ifndef APL_DB_SEASON_REPOSITORY_H
#define APL_DB_SEASON_REPOSITORY_H

#include <stddef.h>
#include <stdint.h>

#include "core/error.h"
#include "db/connection.h"
#include "domain/reference.h"

/* Team-scoped season/period persistence (work item 3.1). Dates are ISO text or
 * empty (stored NULL). Every team-facing operation requires the owning team. */

apl_status apl_season_repository_create(const apl_db_connection *connection, int64_t team_id,
                                        const char *label, const char *start_date,
                                        const char *end_date, const char *notes,
                                        apl_season *out_season, apl_error *error);

apl_status apl_season_repository_get(const apl_db_connection *connection, int64_t team_id,
                                     int64_t id, apl_season *out_season, apl_error *error);

apl_status apl_season_repository_update(const apl_db_connection *connection, int64_t team_id,
                                        int64_t id, const char *label, const char *start_date,
                                        const char *end_date, const char *notes,
                                        apl_season *out_season, apl_error *error);

apl_status apl_season_repository_archive(const apl_db_connection *connection, int64_t team_id,
                                         int64_t id, apl_season *out_season, apl_error *error);

apl_status apl_season_repository_list(const apl_db_connection *connection, int64_t team_id,
                                      bool include_archived, apl_season **out_seasons,
                                      size_t *out_count, apl_error *error);

void apl_season_repository_free_list(apl_season *seasons);

#endif /* APL_DB_SEASON_REPOSITORY_H */
