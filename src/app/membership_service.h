#ifndef APL_APP_MEMBERSHIP_SERVICE_H
#define APL_APP_MEMBERSHIP_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include "app/app.h"
#include "core/error.h"
#include "domain/reference.h"

/* Dated team-driver membership use cases (work item 3.3). */

apl_status apl_membership_service_create(apl_app *app, int64_t team_id, int64_t driver_id,
                                         const char *start_date, const char *end_date,
                                         const char *role, const char *notes,
                                         apl_membership *out_membership, apl_error *error);

apl_status apl_membership_service_get(apl_app *app, int64_t team_id, int64_t id,
                                      apl_membership *out_membership, apl_error *error);

apl_status apl_membership_service_update(apl_app *app, int64_t team_id, int64_t id,
                                         const char *start_date, const char *end_date,
                                         const char *role, const char *notes,
                                         apl_membership *out_membership, apl_error *error);

apl_status apl_membership_service_end(apl_app *app, int64_t team_id, int64_t id,
                                      const char *end_date, apl_membership *out_membership,
                                      apl_error *error);

apl_status apl_membership_service_list_for_team(apl_app *app, int64_t team_id, bool include_ended,
                                                apl_membership **out_memberships, size_t *out_count,
                                                apl_error *error);

apl_status apl_membership_service_list_for_driver(apl_app *app, int64_t driver_id,
                                                  apl_membership **out_memberships,
                                                  size_t *out_count, apl_error *error);

void apl_membership_service_free_list(apl_membership *memberships);

#endif /* APL_APP_MEMBERSHIP_SERVICE_H */
