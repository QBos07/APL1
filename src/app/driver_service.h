#ifndef APL_APP_DRIVER_SERVICE_H
#define APL_APP_DRIVER_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include "app/app.h"
#include "core/error.h"
#include "domain/reference.h"

/* Shared driver identity use cases (work item 3.2). Team roster filtering goes
 * through memberships; duplicate display names remain separate records. */

apl_status apl_driver_service_create(const apl_app *app, const char *name, const char *short_name,
                                     const char *notes, apl_driver *out_driver, apl_error *error);

apl_status apl_driver_service_get(const apl_app *app, int64_t id, apl_driver *out_driver,
                                  apl_error *error);

apl_status apl_driver_service_update(const apl_app *app, int64_t id, const char *name,
                                     const char *short_name, const char *notes,
                                     apl_driver *out_driver, apl_error *error);

apl_status apl_driver_service_archive(const apl_app *app, int64_t id, apl_driver *out_driver,
                                      apl_error *error);

apl_status apl_driver_service_list(const apl_app *app, bool include_archived,
                                   apl_driver **out_drivers, size_t *out_count, apl_error *error);

apl_status apl_driver_service_list_for_team(const apl_app *app, int64_t team_id,
                                            bool include_archived, apl_driver **out_drivers,
                                            size_t *out_count, apl_error *error);

void apl_driver_service_free_list(apl_driver *drivers);

#endif /* APL_APP_DRIVER_SERVICE_H */
