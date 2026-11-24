#include "app/activity_service.h"

#include "db/activity_repository.h"

static apl_status with_context(const apl_status status, const char *const context,
                               apl_error *const error)
{
    if (status != APL_OK) {
        apl_error_prefix(error, status, context);
    }
    return status;
}

apl_status apl_activity_service_list(apl_app *const app, const apl_activity_filter *const filter,
                                     apl_activity_event **const out_events, size_t *const out_count,
                                     apl_error *const error)
{
    if (app == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "application is nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return with_context(
        apl_activity_repository_list(&app->db, filter, out_events, out_count, error),
        "cannot list activity", error);
}

apl_status apl_activity_service_get(apl_app *const app, const int64_t event_id,
                                    apl_activity_event *const out_event,
                                    apl_activity_change **const out_changes,
                                    size_t *const out_change_count, apl_error *const error)
{
    if (app == nullptr || out_event == nullptr || out_changes == nullptr ||
        out_change_count == nullptr || event_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "activity detail arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_changes = nullptr;
    *out_change_count = 0;
    apl_status status = apl_activity_repository_get(&app->db, event_id, out_event, error);
    if (status == APL_OK) {
        status = apl_activity_repository_list_changes(&app->db, event_id, out_changes,
                                                      out_change_count, error);
    }
    return with_context(status, "cannot load activity detail", error);
}

void apl_activity_service_free_events(apl_activity_event *const events)
{
    apl_activity_repository_free_events(events);
}

void apl_activity_service_free_changes(apl_activity_change *const changes)
{
    apl_activity_repository_free_changes(changes);
}
