#ifndef APL_APP_PART_UNIT_SERVICE_H
#define APL_APP_PART_UNIT_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include "app/app.h"
#include "core/error.h"
#include "domain/part.h"

/*
 * Individual part/unit use cases (work item 6.2).
 *
 * All writes validate user-entered text and require a same-team part-type link,
 * then save the unit and its audit event in one transaction. Serial and part
 * numbers and the condition label are genuinely optional; a unit with no serial
 * remains a distinct row by stable ID. No stock or availability state is
 * inferred.
 */

apl_status apl_part_unit_service_create(const apl_app *app, int64_t team_id, int64_t type_id,
                                        const char *serial_number, const char *part_number,
                                        const char *condition, const char *notes,
                                        apl_part_unit *out_unit, apl_error *error);

apl_status apl_part_unit_service_get(const apl_app *app, int64_t team_id, int64_t id,
                                     apl_part_unit *out_unit, apl_error *error);

apl_status apl_part_unit_service_update(const apl_app *app, int64_t team_id, int64_t id,
                                        int64_t type_id, const char *serial_number,
                                        const char *part_number, const char *condition,
                                        const char *notes, apl_part_unit *out_unit,
                                        apl_error *error);

apl_status apl_part_unit_service_archive(const apl_app *app, int64_t team_id, int64_t id,
                                         apl_part_unit *out_unit, apl_error *error);

apl_status apl_part_unit_service_list(const apl_app *app, const apl_part_unit_filter *filter,
                                      apl_part_unit **out_units, size_t *out_count,
                                      apl_error *error);

void apl_part_unit_service_free_list(apl_part_unit *units);

#endif /* APL_APP_PART_UNIT_SERVICE_H */