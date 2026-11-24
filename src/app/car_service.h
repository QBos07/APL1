#ifndef APL_APP_CAR_SERVICE_H
#define APL_APP_CAR_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include "app/app.h"
#include "core/error.h"
#include "domain/reference.h"

/* Team-scoped car/chassis use cases (work item 3.4). */

apl_status apl_car_service_create(apl_app *app, int64_t team_id, const char *label,
                                  const char *chassis, const char *notes, apl_car *out_car,
                                  apl_error *error);

apl_status apl_car_service_get(apl_app *app, int64_t team_id, int64_t id, apl_car *out_car,
                               apl_error *error);

apl_status apl_car_service_update(apl_app *app, int64_t team_id, int64_t id, const char *label,
                                  const char *chassis, const char *notes, apl_car *out_car,
                                  apl_error *error);

apl_status apl_car_service_archive(apl_app *app, int64_t team_id, int64_t id, apl_car *out_car,
                                   apl_error *error);

apl_status apl_car_service_list(apl_app *app, int64_t team_id, bool include_archived,
                                apl_car **out_cars, size_t *out_count, apl_error *error);

void apl_car_service_free_list(apl_car *cars);

#endif /* APL_APP_CAR_SERVICE_H */
