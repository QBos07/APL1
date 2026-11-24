#ifndef APL_APP_ACTIVITY_SERVICE_H
#define APL_APP_ACTIVITY_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include "app/app.h"
#include "core/error.h"
#include "domain/reference.h"

/* Read-only activity/history use cases for the History view (work items
 * 3.5/3.11). The append path is owned by the feature use cases and remains
 * inside their transaction. */

apl_status apl_activity_service_list(apl_app *app, const apl_activity_filter *filter,
                                     apl_activity_event **out_events, size_t *out_count,
                                     apl_error *error);

apl_status apl_activity_service_get(apl_app *app, int64_t event_id, apl_activity_event *out_event,
                                    apl_activity_change **out_changes, size_t *out_change_count,
                                    apl_error *error);

void apl_activity_service_free_events(apl_activity_event *events);
void apl_activity_service_free_changes(apl_activity_change *changes);

#endif /* APL_APP_ACTIVITY_SERVICE_H */
