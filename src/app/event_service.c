#include "app/event_service.h"

#include <stdio.h>
#include <string.h>

#include "app/audit.h"
#include "app/use_case.h"
#include "db/season_repository.h"
#include "domain/reference.h"

/* Validate without changing entered text. The repository stores the same bytes. */
static apl_status validate_event(const int64_t season_id, const char *const name,
                                 const char *const date, const char *const location,
                                 const char *const notes, apl_error *const error)
{
    char checked_name[APL_REFERENCE_LABEL_MAX], checked_date[APL_REFERENCE_DATE_MAX];
    if (season_id < 0 || !name || !date || !location || !notes ||
        strlen(name) >= sizeof(checked_name) || strlen(date) >= sizeof(checked_date) ||
        strlen(location) >= APL_REFERENCE_LABEL_MAX || strlen(notes) >= APL_REFERENCE_NOTES_MAX) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid event fields");
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_status status = apl_reference_label_normalize(name, "event name", checked_name,
                                                      sizeof(checked_name), error);
    if (status != APL_OK)
        return apl_use_case_with_field(status, APL_FIELD_NAME, error);
    status =
        apl_reference_date_normalize(date, "event date", checked_date, sizeof(checked_date), error);
    if (status != APL_OK || checked_date[0] == '\0' || strcmp(date, checked_date) != 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "event date must be a valid ISO date");
        return apl_use_case_with_field(APL_ERR_INVALID_ARGUMENT, APL_FIELD_DATE, error);
    }
    return APL_OK;
}

static apl_status save_event(const apl_app *const app, const int64_t team_id, const int64_t id,
                             const int64_t season_id, const char *const name,
                             const char *const date, const char *const location,
                             const char *const notes, apl_event *const out, apl_error *const error)
{
    if (!app || team_id <= 0 || id < 0 || !out) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid event operation arguments");
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_status status = validate_event(season_id, name, date, location, notes, error);
    if (status != APL_OK)
        return status;
    apl_db_transaction transaction = {};
    status = apl_use_case_begin(&transaction, app, "cannot save event", error);
    if (status != APL_OK)
        return status;
    apl_event old = {};
    if (id)
        status = apl_event_repository_get(&app->db, team_id, id, &old, error);
    if (status == APL_OK && season_id) {
        apl_season season = {};
        status = apl_season_repository_get(&app->db, team_id, season_id, &season, error);
    }
    if (status == APL_OK && id && old.season_id == season_id && strcmp(old.name, name) == 0 &&
        strcmp(old.date, date) == 0 && strcmp(old.location, location) == 0 &&
        strcmp(old.notes, notes) == 0) {
        *out = old;
        return apl_use_case_commit(&transaction, "cannot save event", error);
    }
    apl_event saved = {};
    if (status == APL_OK) {
        status = id ? apl_event_repository_update(&app->db, team_id, id, season_id, name, date,
                                                  location, notes, &saved, error)
                    : apl_event_repository_create(&app->db, team_id, season_id, name, date,
                                                  location, notes, &saved, error);
    }
    if (status == APL_OK) {
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "%s event '%s'", id ? "Updated" : "Created",
                       saved.name);
        status = apl_audit_append(app, team_id, id ? "event_updated" : "event_created", "event",
                                  saved.id, summary, nullptr, 0, error);
    }
    if (status != APL_OK)
        return apl_use_case_rollback(&transaction, status, "cannot save event", error);
    status = apl_use_case_commit(&transaction, "cannot save event", error);
    if (status == APL_OK)
        *out = saved;
    return status;
}

apl_status apl_event_service_create(const apl_app *const app, const int64_t team_id,
                                    const int64_t season_id, const char *const name,
                                    const char *const date, const char *const location,
                                    const char *const notes, apl_event *const out,
                                    apl_error *const error)
{
    return save_event(app, team_id, 0, season_id, name, date, location, notes, out, error);
}

apl_status apl_event_service_update(const apl_app *const app, const int64_t team_id,
                                    const int64_t id, const int64_t season_id,
                                    const char *const name, const char *const date,
                                    const char *const location, const char *const notes,
                                    apl_event *const out, apl_error *const error)
{
    if (id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid event id");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return save_event(app, team_id, id, season_id, name, date, location, notes, out, error);
}

static apl_status save_session(const apl_app *const app, const int64_t team_id,
                               const int64_t event_id, const int64_t id, const char *const label,
                               apl_event_session *const out, apl_error *const error)
{
    char checked[APL_REFERENCE_LABEL_MAX];
    if (!app || team_id <= 0 || event_id <= 0 || id < 0 || !label || !out ||
        strlen(label) >= sizeof(checked)) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid session operation arguments");
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_status status =
        apl_reference_label_normalize(label, "session label", checked, sizeof(checked), error);
    if (status != APL_OK)
        return apl_use_case_with_field(status, APL_FIELD_LABEL, error);
    apl_db_transaction transaction = {};
    status = apl_use_case_begin(&transaction, app, "cannot save session", error);
    if (status != APL_OK)
        return status;
    apl_event event = {};
    status = apl_event_repository_get(&app->db, team_id, event_id, &event, error);
    apl_event_session old = {};
    if (status == APL_OK && id)
        status = apl_session_repository_get(&app->db, team_id, event_id, id, &old, error);
    if (status == APL_OK && id && strcmp(old.label, label) == 0) {
        *out = old;
        return apl_use_case_commit(&transaction, "cannot save session", error);
    }
    apl_event_session saved = {};
    if (status == APL_OK) {
        status =
            id ? apl_session_repository_update(&app->db, team_id, event_id, id, label, &saved,
                                               error)
               : apl_session_repository_create(&app->db, team_id, event_id, label, &saved, error);
    }
    if (status == APL_OK) {
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "%s session '%s'", id ? "Updated" : "Created",
                       saved.label);
        status = apl_audit_append(app, team_id, id ? "session_updated" : "session_created",
                                  "event_session", saved.id, summary, nullptr, 0, error);
    }
    if (status != APL_OK)
        return apl_use_case_rollback(&transaction, status, "cannot save session", error);
    status = apl_use_case_commit(&transaction, "cannot save session", error);
    if (status == APL_OK)
        *out = saved;
    return status;
}

apl_status apl_session_service_create(const apl_app *const app, const int64_t team_id,
                                      const int64_t event_id, const char *const label,
                                      apl_event_session *const out, apl_error *const error)
{
    return save_session(app, team_id, event_id, 0, label, out, error);
}

apl_status apl_session_service_update(const apl_app *const app, const int64_t team_id,
                                      const int64_t event_id, const int64_t id,
                                      const char *const label, apl_event_session *const out,
                                      apl_error *const error)
{
    if (id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid session id");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return save_session(app, team_id, event_id, id, label, out, error);
}
