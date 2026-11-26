#ifndef APL_APP_LIFECYCLE_SERVICE_H
#define APL_APP_LIFECYCLE_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include "app/app.h"
#include "core/error.h"
#include "domain/part.h"

/*
 * Lifecycle use cases (work item 6.5).
 *
 * Install, remove, repair, and dispose validate an active-team scope, the
 * affected same-team unit, and the entered date, then append the lifecycle
 * event and its audit event atomically. Repair never changes fitment; dispose
 * cannot leave a unit recorded as currently fitted. Transition rules live here,
 * not in the schema/repository.
 */

/** Install a unit at an explicit car/position. Rejects if the unit is already
 *  fitted elsewhere, already disposed, or archived. */
apl_status apl_lifecycle_service_install(const apl_app *app, int64_t team_id, int64_t unit_id,
                                         int64_t car_id, const char *position,
                                         const char *event_date, const char *event_label,
                                         const char *reason, const char *usage, const char *notes,
                                         apl_lifecycle_event *out_event, apl_error *error);

/** Remove the currently-fitted unit at the requested car/position. Rejects a
 *  repeated removal or removing a unit not fitted at that location. */
apl_status apl_lifecycle_service_remove(const apl_app *app, int64_t team_id, int64_t unit_id,
                                        int64_t car_id, const char *position,
                                        const char *event_date, const char *event_label,
                                        const char *reason, const char *usage, const char *notes,
                                        apl_lifecycle_event *out_event, apl_error *error);

/** Record a repair that does not change fitment. */
apl_status apl_lifecycle_service_repair(const apl_app *app, int64_t team_id, int64_t unit_id,
                                        int64_t car_id, const char *position,
                                        const char *event_date, const char *event_label,
                                        const char *reason, const char *usage, const char *notes,
                                        apl_lifecycle_event *out_event, apl_error *error);

/** Dispose a unit (must not be currently fitted; require a prior removal). */
apl_status apl_lifecycle_service_dispose(const apl_app *app, int64_t team_id, int64_t unit_id,
                                         const char *event_date, const char *event_label,
                                         const char *reason, const char *usage, const char *notes,
                                         apl_lifecycle_event *out_event, apl_error *error);

/** Replace the currently-fitted `old_unit` at `car`/`position` with `new_unit`
 *  in one atomic operation (work item 6.6).
 *
 * Validates that `old_unit` is the currently recorded fitment and that
 * `new_unit` is eligible (not fitted elsewhere, not disposed, not archived),
 * then appends one replace lifecycle event referencing both units plus its
 * audit event in the same transaction. On any failure the old fitment and all
 * history remain unchanged.
 */
apl_status apl_lifecycle_service_replace(const apl_app *app, int64_t team_id, int64_t car_id,
                                         const char *position, int64_t old_unit_id,
                                         int64_t new_unit_id, const char *event_date,
                                         const char *event_label, const char *reason,
                                         const char *usage, const char *notes,
                                         apl_lifecycle_event *out_event, apl_error *error);

/** Link (or clear, when `transaction_id` is 0) an optional actual cost on a
 *  replacement event (work item 6.7).
 *
 * The link points at one real same-team expense ledger entry; no cost amount is
 * stored on the event. Validation requires the chosen transaction to be an
 * expense in the same team and currency. The link is contextual only: ledger
 * totals still count the transaction once.
 */
apl_status apl_lifecycle_service_set_cost(const apl_app *app, int64_t team_id, int64_t event_id,
                                          int64_t transaction_id, apl_lifecycle_event *out_event,
                                          apl_error *error);

apl_status apl_lifecycle_service_get(const apl_app *app, int64_t team_id, int64_t id,
                                     apl_lifecycle_event *out_event, apl_error *error);

apl_status apl_lifecycle_service_list(const apl_app *app, const apl_lifecycle_filter *filter,
                                      apl_lifecycle_event **out_events, size_t *out_count,
                                      apl_error *error);

apl_status apl_lifecycle_service_list_unit(const apl_app *app, int64_t team_id, int64_t unit_id,
                                           apl_lifecycle_history_entry **out_entries,
                                           size_t *out_count, apl_error *error);

apl_status apl_lifecycle_service_current_fitment(const apl_app *app, int64_t team_id,
                                                 apl_fitment_row **out_rows, size_t *out_count,
                                                 apl_error *error);

void apl_lifecycle_service_free_events(apl_lifecycle_event *events);
void apl_lifecycle_service_free_history(apl_lifecycle_history_entry *entries);
void apl_lifecycle_service_free_fitment(apl_fitment_row *rows);

#endif /* APL_APP_LIFECYCLE_SERVICE_H */