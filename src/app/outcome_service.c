#include "app/outcome_service.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "app/audit.h"
#include "app/use_case.h"
#include "db/membership_repository.h"
#include "db/session_repository.h"

/* Reject overlong encodings, surrogates, and code points above U+10FFFF. */
static bool valid_utf8(const unsigned char *p)
{
    while (*p) {
        if (*p < 0x80) {
            ++p;
            continue;
        }
        unsigned int value;
        unsigned int minimum;
        size_t n;
        if (*p >= 0xc2 && *p <= 0xdf) {
            value = *p & 0x1f;
            minimum = 0x80;
            n = 1;
        } else if (*p >= 0xe0 && *p <= 0xef) {
            value = *p & 0x0f;
            minimum = 0x800;
            n = 2;
        } else if (*p >= 0xf0 && *p <= 0xf4) {
            value = *p & 0x07;
            minimum = 0x10000;
            n = 3;
        } else {
            return false;
        }
        ++p;
        for (size_t i = 0; i < n; ++i) {
            if (*p == 0 || (*p & 0xc0) != 0x80)
                return false;
            value = (value << 6) | (*p++ & 0x3f);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            return false;
    }
    return true;
}

static apl_status validate_text(char *text, size_t size, bool *present, const char *label,
                                apl_error *error)
{
    if (!*present)
        return APL_OK;
    if (!memchr(text, '\0', size) || !valid_utf8((const unsigned char *)text)) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "%s is too long or invalid UTF-8", label);
        return APL_ERR_INVALID_ARGUMENT;
    }
    bool blank = true;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (!isspace(*p)) {
            blank = false;
            break;
        }
    }
    if (blank) {
        *present = false;
        text[0] = '\0';
    }
    return APL_OK;
}

static apl_status validate_values(apl_outcome *input, apl_error *error)
{
    if ((input->has_start_position && input->start_position <= 0) ||
        (input->has_finish_position && input->finish_position <= 0) ||
        (input->has_laps && input->laps < 0)) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "positions must be positive and laps nonnegative");
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_status status =
        validate_text(input->status, sizeof(input->status), &input->has_status, "status", error);
    if (status == APL_OK)
        status =
            validate_text(input->notes, sizeof(input->notes), &input->has_notes, "notes", error);
    return status;
}

static apl_status validate_target(const apl_app *app, const apl_outcome *input, apl_error *error)
{
    apl_event_session session = {};
    apl_status status = apl_session_repository_get(&app->db, input->team_id, input->event_id,
                                                   input->session_id, &session, error);
    if (status != APL_OK)
        return status;
    apl_membership *memberships = nullptr;
    size_t count = 0;
    status = apl_membership_repository_list_for_driver(&app->db, input->driver_id, &memberships,
                                                       &count, error);
    if (status != APL_OK)
        return status;
    bool belongs = false;
    for (size_t i = 0; i < count; ++i)
        if (memberships[i].team_id == input->team_id)
            belongs = true;
    apl_membership_repository_free_list(memberships);
    if (!belongs) {
        apl_error_set(error, APL_ERR_NOT_FOUND, "driver has no membership for this team");
        return APL_ERR_NOT_FOUND;
    }
    return APL_OK;
}

apl_status apl_outcome_service_save(const apl_app *app, apl_outcome_save_mode mode,
                                    const apl_outcome *input, apl_outcome *out, apl_error *error)
{
    if (!app || !input || !out || (mode != APL_OUTCOME_CREATE && mode != APL_OUTCOME_EDIT) ||
        input->team_id <= 0 || input->event_id <= 0 || input->session_id <= 0 ||
        input->driver_id <= 0 || (mode == APL_OUTCOME_EDIT && input->id <= 0)) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid outcome save arguments");
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_outcome normalized = *input;
    apl_status status = validate_values(&normalized, error);
    if (status != APL_OK)
        return status;
    apl_db_transaction transaction = {};
    status = apl_use_case_begin(&transaction, app, "cannot save outcome", error);
    if (status != APL_OK)
        return status;
    status = validate_target(app, &normalized, error);
    if (status == APL_OK && mode == APL_OUTCOME_EDIT) {
        apl_outcome old = {};
        status =
            apl_outcome_repository_get(&app->db, normalized.team_id, normalized.id, &old, error);
        if (status == APL_OK &&
            (old.event_id != normalized.event_id || old.session_id != normalized.session_id ||
             old.driver_id != normalized.driver_id)) {
            apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "an outcome target cannot be changed");
            status = APL_ERR_INVALID_ARGUMENT;
        }
    }
    apl_outcome saved = {};
    if (status == APL_OK)
        status = mode == APL_OUTCOME_CREATE
                     ? apl_outcome_repository_create(&app->db, &normalized, &saved, error)
                     : apl_outcome_repository_update(&app->db, normalized.team_id, normalized.id,
                                                     &normalized, &saved, error);
    if (status == APL_OK) {
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "%s outcome for driver %lld in session %lld",
                       mode == APL_OUTCOME_CREATE ? "Created" : "Updated",
                       (long long)saved.driver_id, (long long)saved.session_id);
        status = apl_audit_append(
            app, saved.team_id, mode == APL_OUTCOME_CREATE ? "outcome_created" : "outcome_updated",
            "driver_outcome", saved.id, summary, nullptr, 0, error);
    }
    if (status != APL_OK)
        return apl_use_case_rollback(&transaction, status, "cannot save outcome", error);
    status = apl_use_case_commit(&transaction, "cannot save outcome", error);
    if (status == APL_OK)
        *out = saved;
    return status;
}
