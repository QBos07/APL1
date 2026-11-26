#ifndef APL_DB_LIFECYCLE_REPOSITORY_H
#define APL_DB_LIFECYCLE_REPOSITORY_H

#include <stddef.h>
#include <stdint.h>

#include "core/error.h"
#include "db/connection.h"
#include "domain/part.h"

/* Lifecycle-event persistence and derived-fitment queries (work item 6.4).
 *
 * Events are append-only: the repository exposes append/get/list but no
 * update/delete. Team scope is inherited from the affected units and cars via
 * JOINs, so every read passes an explicit team ID. Current fitment is derived
 * from recorded install/remove/replace/repair/dispose events, never from a
 * mutable field or the absence of history.
 */

apl_status apl_lifecycle_repository_append(const apl_db_connection *connection, int64_t team_id,
                                           const apl_lifecycle_event *event,
                                           apl_lifecycle_event *out_event, apl_error *error);

apl_status apl_lifecycle_repository_get(const apl_db_connection *connection, int64_t team_id,
                                        int64_t id, apl_lifecycle_event *out_event,
                                        apl_error *error);

/** List events for a team (optionally scoped by unit, car, action, and date
 *  range) in entered-date order with stable ID tie-breaks. */
apl_status apl_lifecycle_repository_list(const apl_db_connection *connection,
                                         const apl_lifecycle_filter *filter,
                                         apl_lifecycle_event **out_events, size_t *out_count,
                                         apl_error *error);

/** List one unit's events in entered-date order, decorating each with whether
 *  it represents the currently-derived fitted state for its car/position. */
apl_status apl_lifecycle_repository_list_unit(const apl_db_connection *connection, int64_t team_id,
                                              int64_t unit_id,
                                              apl_lifecycle_history_entry **out_entries,
                                              size_t *out_count, apl_error *error);

/** Compute the current derived fitment for a team: one row per currently-fitted
 *  unit with its car/position. Unknown/unrecorded positions are absent. */
apl_status apl_lifecycle_repository_current_fitment(const apl_db_connection *connection,
                                                    int64_t team_id, apl_fitment_row **out_rows,
                                                    size_t *out_count, apl_error *error);

void apl_lifecycle_repository_free_events(apl_lifecycle_event *events);
void apl_lifecycle_repository_free_history(apl_lifecycle_history_entry *entries);
void apl_lifecycle_repository_free_fitment(apl_fitment_row *rows);

#endif /* APL_DB_LIFECYCLE_REPOSITORY_H */