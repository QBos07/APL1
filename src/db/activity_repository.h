#ifndef APL_DB_ACTIVITY_REPOSITORY_H
#define APL_DB_ACTIVITY_REPOSITORY_H

#include <stddef.h>
#include <stdint.h>

#include "core/error.h"
#include "db/connection.h"
#include "domain/reference.h"

/*
 * Append-only activity/audit persistence (work items 3.5/3.7).
 *
 * The repository intentionally exposes no update or delete operation. An event
 * and all of its field changes are written by one append call; the caller owns
 * the surrounding transaction so a failed feature write and its audit row roll
 * back together. Occurrence timestamps are system-generated UTC text, not
 * user-entered record dates.
 */

/** Append one event and its changes. Returns the stored event in `out_event`
 *  (optional). For a missing old/new value set the corresponding `*_missing`
 *  flag on the change. */
apl_status apl_activity_repository_append(apl_db_connection *connection,
                                          const apl_activity_event *event,
                                          const apl_activity_change *changes, size_t change_count,
                                          apl_activity_event *out_event, apl_error *error);

/** Fetch one activity event by stable ID. */
apl_status apl_activity_repository_get(apl_db_connection *connection, int64_t id,
                                       apl_activity_event *out_event, apl_error *error);

/** List events newest first using the optional filter. */
apl_status apl_activity_repository_list(apl_db_connection *connection,
                                        const apl_activity_filter *filter,
                                        apl_activity_event **out_events, size_t *out_count,
                                        apl_error *error);

/** Load field changes for one event in insertion order. */
apl_status apl_activity_repository_list_changes(apl_db_connection *connection, int64_t event_id,
                                                apl_activity_change **out_changes,
                                                size_t *out_count, apl_error *error);

void apl_activity_repository_free_events(apl_activity_event *events);
void apl_activity_repository_free_changes(apl_activity_change *changes);

#endif /* APL_DB_ACTIVITY_REPOSITORY_H */
