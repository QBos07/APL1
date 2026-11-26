#ifndef APL_DB_PART_UNIT_REPOSITORY_H
#define APL_DB_PART_UNIT_REPOSITORY_H

#include <stddef.h>
#include <stdint.h>

#include "core/error.h"
#include "db/connection.h"
#include "domain/part.h"

/* Team-scoped individual part/unit persistence (work item 6.2).
 *
 * A unit requires a part-type link belonging to the same team (enforced by a
 * trigger). Serial/part numbers and the condition label are optional. Every
 * read requires an explicit team ID. Archived rows stay readable through
 * get/list with explicit archived inclusion.
 */

apl_status apl_part_unit_repository_create(const apl_db_connection *connection,
                                           const apl_part_unit *unit, apl_part_unit *out_unit,
                                           apl_error *error);

apl_status apl_part_unit_repository_get(const apl_db_connection *connection, int64_t team_id,
                                        int64_t id, apl_part_unit *out_unit, apl_error *error);

/** Replace the mutable fields of an existing unit in the same team. */
apl_status apl_part_unit_repository_update(const apl_db_connection *connection,
                                           const apl_part_unit *unit, apl_part_unit *out_unit,
                                           apl_error *error);

apl_status apl_part_unit_repository_archive(const apl_db_connection *connection, int64_t team_id,
                                            int64_t id, apl_part_unit *out_unit, apl_error *error);

/** List units ordered by serial then part number then stable ID (archived
 *  last). When `filter->type_id` is positive the list is scoped to that type. */
apl_status apl_part_unit_repository_list(const apl_db_connection *connection,
                                         const apl_part_unit_filter *filter,
                                         apl_part_unit **out_units, size_t *out_count,
                                         apl_error *error);

void apl_part_unit_repository_free_list(apl_part_unit *units);

#endif /* APL_DB_PART_UNIT_REPOSITORY_H */