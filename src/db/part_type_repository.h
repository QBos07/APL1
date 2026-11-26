#ifndef APL_DB_PART_TYPE_REPOSITORY_H
#define APL_DB_PART_TYPE_REPOSITORY_H

#include <stddef.h>
#include <stdint.h>

#include "core/error.h"
#include "db/connection.h"
#include "domain/part.h"

/* Team-scoped component-type persistence (work item 6.1).
 *
 * Every read requires an explicit team ID. Category and name are user-entered
 * labels; no catalogue or compatibility data is stored. The repository enforces
 * the duplicate-name policy within a team/category (case-insensitively) through
 * a partial unique index that excludes archived rows, letting an archived type
 * free its name without erasing historical units/history.
 */

apl_status apl_part_type_repository_create(const apl_db_connection *connection,
                                           const apl_part_type *type, apl_part_type *out_type,
                                           apl_error *error);

apl_status apl_part_type_repository_get(const apl_db_connection *connection, int64_t team_id,
                                        int64_t id, apl_part_type *out_type, apl_error *error);

/** Replace the mutable fields of an existing type in the same team. */
apl_status apl_part_type_repository_update(const apl_db_connection *connection,
                                           const apl_part_type *type, apl_part_type *out_type,
                                           apl_error *error);

apl_status apl_part_type_repository_archive(const apl_db_connection *connection, int64_t team_id,
                                            int64_t id, apl_part_type *out_type, apl_error *error);

/** List types ordered by category then name (case-insensitive) then stable ID. */
apl_status apl_part_type_repository_list(const apl_db_connection *connection,
                                         const apl_part_type_filter *filter,
                                         apl_part_type **out_types, size_t *out_count,
                                         apl_error *error);

void apl_part_type_repository_free_list(apl_part_type *types);

#endif /* APL_DB_PART_TYPE_REPOSITORY_H */