#ifndef APL_APP_PART_TYPE_SERVICE_H
#define APL_APP_PART_TYPE_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include "app/app.h"
#include "core/error.h"
#include "domain/part.h"

/*
 * Part-type use cases (work item 6.1).
 *
 * All writes validate user-entered category/name text, then save the type and
 * its audit event in one transaction. Category and name remain free-form user
 * labels with no seeded catalogue or compatibility rules. Archiving a type
 * preserves existing units/history while freeing its name for reuse.
 */

apl_status apl_part_type_service_create(const apl_app *app, int64_t team_id, const char *category,
                                        const char *name, const char *description,
                                        const char *notes, apl_part_type *out_type,
                                        apl_error *error);

apl_status apl_part_type_service_get(const apl_app *app, int64_t team_id, int64_t id,
                                     apl_part_type *out_type, apl_error *error);

apl_status apl_part_type_service_update(const apl_app *app, int64_t team_id, int64_t id,
                                        const char *category, const char *name,
                                        const char *description, const char *notes,
                                        apl_part_type *out_type, apl_error *error);

apl_status apl_part_type_service_archive(const apl_app *app, int64_t team_id, int64_t id,
                                         apl_part_type *out_type, apl_error *error);

apl_status apl_part_type_service_list(const apl_app *app, const apl_part_type_filter *filter,
                                      apl_part_type **out_types, size_t *out_count,
                                      apl_error *error);

void apl_part_type_service_free_list(apl_part_type *types);

#endif /* APL_APP_PART_TYPE_SERVICE_H */