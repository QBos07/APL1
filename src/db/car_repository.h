#ifndef APL_DB_CAR_REPOSITORY_H
#define APL_DB_CAR_REPOSITORY_H

#include <stddef.h>
#include <stdint.h>

#include "core/error.h"
#include "db/connection.h"
#include "domain/reference.h"

/* Team-scoped car/chassis reference persistence (work item 3.4). */

apl_status apl_car_repository_create(apl_db_connection *connection, int64_t team_id,
                                     const char *label, const char *chassis, const char *notes,
                                     apl_car *out_car, apl_error *error);

apl_status apl_car_repository_get(apl_db_connection *connection, int64_t team_id, int64_t id,
                                  apl_car *out_car, apl_error *error);

apl_status apl_car_repository_update(apl_db_connection *connection, int64_t team_id, int64_t id,
                                     const char *label, const char *chassis, const char *notes,
                                     apl_car *out_car, apl_error *error);

apl_status apl_car_repository_archive(apl_db_connection *connection, int64_t team_id, int64_t id,
                                      apl_car *out_car, apl_error *error);

apl_status apl_car_repository_list(apl_db_connection *connection, int64_t team_id,
                                   bool include_archived, apl_car **out_cars, size_t *out_count,
                                   apl_error *error);

void apl_car_repository_free_list(apl_car *cars);

#endif /* APL_DB_CAR_REPOSITORY_H */
