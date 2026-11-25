#include "app/audit.h"

#include <stdio.h>

#include "db/activity_repository.h"

apl_status apl_audit_append(const apl_app *const app, const int64_t team_id,
                            const char *const action, const char *const entity_type,
                            const int64_t entity_id, const char *const summary,
                            const apl_activity_change *const changes, const size_t change_count,
                            apl_error *const error)
{
    if (app == nullptr || action == nullptr || entity_type == nullptr || summary == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "audit append arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_activity_event event = {};
    event.team_id = team_id;
    event.entity_id = entity_id;
    (void)snprintf(event.action, sizeof(event.action), "%s", action);
    (void)snprintf(event.entity_type, sizeof(event.entity_type), "%s", entity_type);
    (void)snprintf(event.summary, sizeof(event.summary), "%s", summary);
    return apl_activity_repository_append(&app->db, &event, changes, change_count, nullptr, error);
}

void apl_audit_change_text(apl_activity_change *const change, const char *const field,
                           const char *const old_value, const bool old_missing,
                           const char *const new_value, const bool new_missing)
{
    if (change == nullptr) {
        return;
    }
    *change = (apl_activity_change){};
    (void)snprintf(change->field, sizeof(change->field), "%s", (field != nullptr) ? field : "");
    change->old_missing = old_missing;
    change->new_missing = new_missing;
    if (!old_missing && old_value != nullptr) {
        (void)snprintf(change->old_value, sizeof(change->old_value), "%s", old_value);
    }
    if (!new_missing && new_value != nullptr) {
        (void)snprintf(change->new_value, sizeof(change->new_value), "%s", new_value);
    }
}

void apl_audit_change_bool(apl_activity_change *const change, const char *const field,
                           const bool old_value, const bool new_value)
{
    apl_audit_change_text(change, field, old_value ? "true" : "false", false,
                          new_value ? "true" : "false", false);
}
