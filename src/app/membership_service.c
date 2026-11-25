#include "app/membership_service.h"

#include <stdio.h>
#include <string.h>

#include "app/audit.h"
#include "app/use_case.h"
#include "db/driver_repository.h"
#include "db/membership_repository.h"
#include "db/team_repository.h"
#include "db/transaction.h"

/** Verify both references and reject a new current membership for an archived
 *  driver. Historical memberships may still be edited after archiving. */
static apl_status verify_references(const apl_app *const app, const int64_t team_id,
                                    const int64_t driver_id, const bool require_current_eligible,
                                    apl_error *const error)
{
    apl_team team = {};
    apl_status status = apl_team_repository_get(&app->db, team_id, &team, error);
    if (status != APL_OK) {
        return status;
    }
    if (team.archived) {
        apl_error_set(error, APL_ERR_NOT_FOUND, "archived teams cannot receive new memberships");
        return APL_ERR_NOT_FOUND;
    }

    apl_driver driver = {};
    status = apl_driver_repository_get(&app->db, driver_id, &driver, error);
    if (status != APL_OK) {
        return status;
    }
    if (require_current_eligible && driver.archived) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "archived drivers cannot start a new current membership");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return APL_OK;
}

static apl_status normalize_fields(const char *const start_date, const char *const end_date,
                                   const char *const role, const char *const notes,
                                   char *const out_start, char *const out_end, char *const out_role,
                                   char *const out_notes, apl_error *const error)
{
    apl_status status =
        apl_membership_dates_normalize(start_date, end_date, out_start, APL_REFERENCE_DATE_MAX,
                                       out_end, APL_REFERENCE_DATE_MAX, error);
    if (status == APL_OK) {
        status = apl_reference_optional_normalize(role, "membership role", out_role,
                                                  APL_REFERENCE_SHORT_MAX, error);
    }
    if (status == APL_OK) {
        status = apl_reference_optional_normalize(notes, "membership notes", out_notes,
                                                  APL_REFERENCE_NOTES_MAX, error);
    }
    return status;
}

static apl_status append_event(const apl_app *const app, const int64_t team_id,
                               const char *const action, const int64_t membership_id,
                               const char *const summary, const apl_activity_change *const changes,
                               const size_t count, apl_error *const error)
{
    return apl_audit_append(app, team_id, action, "membership", membership_id, summary, changes,
                            count, error);
}

apl_status apl_membership_service_create(const apl_app *const app, const int64_t team_id,
                                         const int64_t driver_id, const char *const start_date,
                                         const char *const end_date, const char *const role,
                                         const char *const notes,
                                         apl_membership *const out_membership,
                                         apl_error *const error)
{
    if (app == nullptr || out_membership == nullptr || team_id <= 0 || driver_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "membership create arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    char normalized_start[APL_REFERENCE_DATE_MAX];
    char normalized_end[APL_REFERENCE_DATE_MAX];
    char normalized_role[APL_REFERENCE_SHORT_MAX];
    char normalized_notes[APL_REFERENCE_NOTES_MAX];
    apl_status status = normalize_fields(start_date, end_date, role, notes, normalized_start,
                                         normalized_end, normalized_role, normalized_notes, error);
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot add membership", error);
    }
    const bool current = normalized_end[0] == '\0';
    status = verify_references(app, team_id, driver_id, current, error);
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot add membership", error);
    }

    apl_db_transaction transaction = {};
    status = apl_use_case_begin(&transaction, app, "cannot add membership", error);
    if (status != APL_OK) {
        return status;
    }
    status = apl_membership_repository_create(&app->db, team_id, driver_id, normalized_start,
                                              normalized_end, normalized_role, normalized_notes,
                                              out_membership, error);
    if (status == APL_OK) {
        apl_activity_change changes[4] = {};
        apl_audit_change_text(&changes[0], "start_date", nullptr, true, out_membership->start_date,
                              false);
        apl_audit_change_text(&changes[1], "end_date", nullptr, true, out_membership->end_date,
                              out_membership->end_date[0] == '\0');
        apl_audit_change_text(&changes[2], "role", nullptr, true, out_membership->role, false);
        apl_audit_change_text(&changes[3], "notes", nullptr, true, out_membership->notes, false);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Added team membership starting %s",
                       out_membership->start_date);
        status = append_event(app, team_id, "membership_added", out_membership->id, summary,
                              changes, 4, error);
    }
    if (status != APL_OK) {
        return apl_use_case_rollback(&transaction, status, "cannot add membership", error);
    }
    return apl_use_case_commit(&transaction, "cannot add membership", error);
}

apl_status apl_membership_service_get(const apl_app *const app, const int64_t team_id,
                                      const int64_t id, apl_membership *const out_membership,
                                      apl_error *const error)
{
    if (app == nullptr || out_membership == nullptr || team_id <= 0 || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "membership get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(
        apl_membership_repository_get(&app->db, team_id, id, out_membership, error),
        "cannot load membership", error);
}

apl_status apl_membership_service_update(const apl_app *const app, const int64_t team_id,
                                         const int64_t id, const char *const start_date,
                                         const char *const end_date, const char *const role,
                                         const char *const notes,
                                         apl_membership *const out_membership,
                                         apl_error *const error)
{
    if (app == nullptr || out_membership == nullptr || team_id <= 0 || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "membership update arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    char normalized_start[APL_REFERENCE_DATE_MAX];
    char normalized_end[APL_REFERENCE_DATE_MAX];
    char normalized_role[APL_REFERENCE_SHORT_MAX];
    char normalized_notes[APL_REFERENCE_NOTES_MAX];
    apl_status status = normalize_fields(start_date, end_date, role, notes, normalized_start,
                                         normalized_end, normalized_role, normalized_notes, error);
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot update membership", error);
    }

    apl_db_transaction transaction = {};
    status = apl_use_case_begin(&transaction, app, "cannot update membership", error);
    if (status != APL_OK) {
        return status;
    }
    apl_membership existing = {};
    status = apl_membership_repository_get(&app->db, team_id, id, &existing, error);
    if (status != APL_OK) {
        return apl_use_case_rollback(&transaction, status, "cannot update membership", error);
    }
    const bool start_changed = strcmp(existing.start_date, normalized_start) != 0;
    const bool end_changed = strcmp(existing.end_date, normalized_end) != 0;
    const bool role_changed = strcmp(existing.role, normalized_role) != 0;
    const bool notes_changed = strcmp(existing.notes, normalized_notes) != 0;
    if (!start_changed && !end_changed && !role_changed && !notes_changed) {
        *out_membership = existing;
        return apl_use_case_commit(&transaction, "cannot update membership", error);
    }
    status =
        apl_membership_repository_update(&app->db, team_id, id, normalized_start, normalized_end,
                                         normalized_role, normalized_notes, out_membership, error);
    if (status == APL_OK) {
        apl_activity_change changes[4] = {};
        size_t count = 0;
        if (start_changed) {
            apl_audit_change_text(&changes[count++], "start_date", existing.start_date, false,
                                  out_membership->start_date, false);
        }
        if (end_changed) {
            apl_audit_change_text(&changes[count++], "end_date", existing.end_date,
                                  existing.end_date[0] == '\0', out_membership->end_date,
                                  out_membership->end_date[0] == '\0');
        }
        if (role_changed) {
            apl_audit_change_text(&changes[count++], "role", existing.role, false,
                                  out_membership->role, false);
        }
        if (notes_changed) {
            apl_audit_change_text(&changes[count++], "notes", existing.notes, false,
                                  out_membership->notes, false);
        }
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Updated team membership starting %s",
                       out_membership->start_date);
        status =
            append_event(app, team_id, "membership_updated", id, summary, changes, count, error);
    }
    if (status != APL_OK) {
        return apl_use_case_rollback(&transaction, status, "cannot update membership", error);
    }
    return apl_use_case_commit(&transaction, "cannot update membership", error);
}

apl_status apl_membership_service_end(const apl_app *const app, const int64_t team_id,
                                      const int64_t id, const char *const end_date,
                                      apl_membership *const out_membership, apl_error *const error)
{
    if (app == nullptr || out_membership == nullptr || team_id <= 0 || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "membership end arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    char normalized_end[APL_REFERENCE_DATE_MAX];
    apl_status status = apl_reference_date_normalize(end_date, "membership end date",
                                                     normalized_end, APL_REFERENCE_DATE_MAX, error);
    if (status == APL_OK && normalized_end[0] == '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "membership end date is required");
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot end membership", error);
    }

    apl_db_transaction transaction = {};
    status = apl_use_case_begin(&transaction, app, "cannot end membership", error);
    if (status != APL_OK) {
        return status;
    }
    apl_membership existing = {};
    status = apl_membership_repository_get(&app->db, team_id, id, &existing, error);
    if (status == APL_OK) {
        status = apl_reference_date_range_validate(existing.start_date, normalized_end,
                                                   "membership", error);
    }
    if (status == APL_OK && existing.end_date[0] != '\0' &&
        strcmp(existing.end_date, normalized_end) == 0) {
        *out_membership = existing;
        return apl_use_case_commit(&transaction, "cannot end membership", error);
    }
    if (status == APL_OK) {
        status = apl_membership_repository_end(&app->db, team_id, id, normalized_end,
                                               out_membership, error);
    }
    if (status == APL_OK) {
        apl_activity_change change = {};
        apl_audit_change_text(&change, "end_date", existing.end_date, existing.end_date[0] == '\0',
                              out_membership->end_date, false);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Ended team membership on %s",
                       out_membership->end_date);
        status = append_event(app, team_id, "membership_ended", id, summary, &change, 1, error);
    }
    if (status != APL_OK) {
        return apl_use_case_rollback(&transaction, status, "cannot end membership", error);
    }
    return apl_use_case_commit(&transaction, "cannot end membership", error);
}

apl_status apl_membership_service_list_for_team(const apl_app *const app, const int64_t team_id,
                                                const bool include_ended,
                                                apl_membership **const out_memberships,
                                                size_t *const out_count, apl_error *const error)
{
    if (app == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "membership list arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(
        apl_membership_repository_list_for_team(&app->db, team_id, include_ended, out_memberships,
                                                out_count, error),
        "cannot list memberships", error);
}

apl_status apl_membership_service_list_for_driver(const apl_app *const app, const int64_t driver_id,
                                                  apl_membership **const out_memberships,
                                                  size_t *const out_count, apl_error *const error)
{
    if (app == nullptr || driver_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "driver membership list arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(apl_membership_repository_list_for_driver(
                                         &app->db, driver_id, out_memberships, out_count, error),
                                     "cannot list driver memberships", error);
}

void apl_membership_service_free_list(apl_membership *const memberships)
{
    apl_membership_repository_free_list(memberships);
}
