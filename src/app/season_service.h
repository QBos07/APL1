#ifndef APL_APP_SEASON_SERVICE_H
#define APL_APP_SEASON_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include "app/app.h"
#include "core/error.h"
#include "domain/reference.h"

/* Team-scoped season/period use cases (work item 3.1). */

apl_status apl_season_service_create(const apl_app *app, int64_t team_id, const char *label,
                                     const char *start_date, const char *end_date,
                                     const char *notes, apl_season *out_season, apl_error *error);

apl_status apl_season_service_get(const apl_app *app, int64_t team_id, int64_t id,
                                  apl_season *out_season, apl_error *error);

apl_status apl_season_service_update(const apl_app *app, int64_t team_id, int64_t id,
                                     const char *label, const char *start_date,
                                     const char *end_date, const char *notes,
                                     apl_season *out_season, apl_error *error);

apl_status apl_season_service_archive(const apl_app *app, int64_t team_id, int64_t id,
                                      apl_season *out_season, apl_error *error);

apl_status apl_season_service_list(const apl_app *app, int64_t team_id, bool include_archived,
                                   apl_season **out_seasons, size_t *out_count, apl_error *error);

void apl_season_service_free_list(apl_season *seasons);

#endif /* APL_APP_SEASON_SERVICE_H */
