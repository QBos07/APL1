#include "app/team_service.h"

#include <stdio.h>
#include <string.h>

#include "app/use_case.h"
#include "app/audit.h"
#include "db/activity_repository.h"
#include "db/budget_repository.h"
#include "db/ledger_repository.h"
#include "db/settings_repository.h"
#include "db/team_repository.h"
#include "db/transaction.h"
#include "domain/ledger.h"
#include "domain/record.h"

/** Append one team activity event through the repository. The caller owns the
 *  transaction, so team, settings, and audit rows commit together. */
static apl_status append_team_event(const apl_app *const app, const int64_t team_id,
                                    const char *const action, const char *const summary,
                                    const apl_activity_change *const changes,
                                    const size_t change_count, apl_error *const error)
{
    apl_activity_event event = {};
    event.team_id = team_id;
    event.entity_id = team_id;
    (void)snprintf(event.action, sizeof(event.action), "%s", action);
    (void)snprintf(event.entity_type, sizeof(event.entity_type), "%s",
                   apl_record_kind_code(APL_RECORD_TEAM));
    (void)snprintf(event.summary, sizeof(event.summary), "%s", summary);
    return apl_activity_repository_append(&app->db, &event, changes, change_count, nullptr, error);
}

static void set_change_text(apl_activity_change *const change, const char *const field,
                            const char *const old_value, const bool old_missing,
                            const char *const new_value, const bool new_missing)
{
    *change = (apl_activity_change){};
    (void)snprintf(change->field, sizeof(change->field), "%s", field);
    change->old_missing = old_missing;
    change->new_missing = new_missing;
    if (!old_missing) {
        (void)snprintf(change->old_value, sizeof(change->old_value), "%s", old_value);
    }
    if (!new_missing) {
        (void)snprintf(change->new_value, sizeof(change->new_value), "%s", new_value);
    }
}

apl_status apl_team_service_create(const apl_app *const app, const char *const name,
                                   const char *const notes, apl_team *const out_team,
                                   apl_error *const error)
{
    if (app == nullptr || out_team == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "team create arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }

    char normalized_name[APL_TEAM_NAME_MAX];
    char normalized_notes[APL_TEAM_NOTES_MAX];
    apl_status status = apl_use_case_with_field(
        apl_team_name_normalize(name, normalized_name, sizeof(normalized_name), error),
        APL_FIELD_NAME, error);
    if (status == APL_OK) {
        status = apl_use_case_with_field(
            apl_team_notes_normalize(notes, normalized_notes, sizeof(normalized_notes), error),
            APL_FIELD_NOTES, error);
    }
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot create team", error);
    }

    apl_db_transaction transaction = {};
    status = apl_use_case_begin(&transaction, app, "cannot create team", error);
    if (status != APL_OK) {
        return status;
    }

    status =
        apl_team_repository_create(&app->db, normalized_name, normalized_notes, out_team, error);
    if (status == APL_OK) {
        status = apl_settings_repository_set_active_team_id(&app->db, out_team->id, error);
    }
    if (status == APL_OK) {
        apl_activity_change changes[2] = {};
        set_change_text(&changes[0], "name", "", true, out_team->name, false);
        set_change_text(&changes[1], "notes", "", true, out_team->notes, false);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Created team '%s'", out_team->name);
        status = append_team_event(app, out_team->id, "team_created", summary, changes, 2, error);
    }

    if (status != APL_OK) {
        const apl_status original = status;
        return apl_use_case_rollback(&transaction, original, "cannot create team", error);
    }

    return apl_use_case_commit(&transaction, "cannot create team", error);
}

apl_status apl_team_service_list(const apl_app *const app, const bool include_archived,
                                 apl_team **const out_teams, size_t *const out_count,
                                 apl_error *const error)
{
    if (app == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "application is nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(
        apl_team_repository_list(&app->db, include_archived, out_teams, out_count, error),
        "cannot list teams", error);
}

apl_status apl_team_service_get(const apl_app *const app, const int64_t id,
                                apl_team *const out_team, apl_error *const error)
{
    if (app == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "application is nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(apl_team_repository_get(&app->db, id, out_team, error),
                                     "cannot load team", error);
}

apl_status apl_team_service_update(const apl_app *const app, const int64_t id,
                                   const char *const name, const char *const notes,
                                   apl_team *const out_team, apl_error *const error)
{
    if (app == nullptr || out_team == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "team update arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }

    char normalized_name[APL_TEAM_NAME_MAX];
    char normalized_notes[APL_TEAM_NOTES_MAX];
    apl_status status = apl_use_case_with_field(
        apl_team_name_normalize(name, normalized_name, sizeof(normalized_name), error),
        APL_FIELD_NAME, error);
    if (status == APL_OK) {
        status = apl_use_case_with_field(
            apl_team_notes_normalize(notes, normalized_notes, sizeof(normalized_notes), error),
            APL_FIELD_NOTES, error);
    }
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot update team", error);
    }

    apl_db_transaction transaction = {};
    status = apl_use_case_begin(&transaction, app, "cannot update team", error);
    if (status != APL_OK) {
        return status;
    }

    apl_team existing = {};
    status = apl_team_repository_get(&app->db, id, &existing, error);
    if (status != APL_OK) {
        const apl_status original = status;
        return apl_use_case_rollback(&transaction, original, "cannot update team", error);
    }

    const bool name_changed = strcmp(existing.name, normalized_name) != 0;
    const bool notes_changed = strcmp(existing.notes, normalized_notes) != 0;
    if (!name_changed && !notes_changed) {
        *out_team = existing;
        return apl_use_case_commit(&transaction, "cannot update team", error);
    }

    status = apl_team_repository_update(&app->db, id, normalized_name, normalized_notes, out_team,
                                        error);
    if (status == APL_OK) {
        apl_activity_change changes[2] = {};
        size_t change_count = 0;
        if (name_changed) {
            set_change_text(&changes[change_count++], "name", existing.name, false, out_team->name,
                            false);
        }
        if (notes_changed) {
            set_change_text(&changes[change_count++], "notes", existing.notes, false,
                            out_team->notes, false);
        }
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        if (name_changed) {
            (void)snprintf(summary, sizeof(summary), "Renamed team '%s' to '%s'", existing.name,
                           out_team->name);
        } else {
            (void)snprintf(summary, sizeof(summary), "Updated profile of team '%s'",
                           out_team->name);
        }
        status = append_team_event(app, id, "team_renamed", summary, changes, change_count, error);
    }

    if (status != APL_OK) {
        const apl_status original = status;
        return apl_use_case_rollback(&transaction, original, "cannot update team", error);
    }
    return apl_use_case_commit(&transaction, "cannot update team", error);
}

/** Choose the oldest-created non-archived team, with stable ID as tie-breaker. */
static apl_status find_oldest_active_team(const apl_db_connection *const connection,
                                          apl_team *const out_team, bool *const out_found,
                                          apl_error *const error)
{
    *out_found = false;
    apl_team *teams = nullptr;
    size_t count = 0;
    apl_status status = apl_team_repository_list(connection, false, &teams, &count, error);
    if (status != APL_OK) {
        return status;
    }

    const apl_team *best = nullptr;
    for (size_t index = 0; index < count; ++index) {
        if (best == nullptr || strcmp(teams[index].meta.created_at, best->meta.created_at) < 0 ||
            (strcmp(teams[index].meta.created_at, best->meta.created_at) == 0 &&
             teams[index].id < best->id)) {
            best = &teams[index];
        }
    }
    if (best != nullptr) {
        *out_team = *best;
        *out_found = true;
    }
    apl_team_repository_free_list(teams);
    return APL_OK;
}

apl_status apl_team_service_archive(const apl_app *const app, const int64_t id,
                                    apl_team *const out_team, apl_error *const error)
{
    if (app == nullptr || out_team == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "team archive arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }

    apl_db_transaction transaction = {};
    apl_status status = apl_use_case_begin(&transaction, app, "cannot archive team", error);
    if (status != APL_OK) {
        return status;
    }

    apl_team existing = {};
    status = apl_team_repository_get(&app->db, id, &existing, error);
    if (status == APL_OK && existing.meta.archived) {
        /* Archive is idempotent: do not write a second misleading activity
         * event or change the original archived_at timestamp. */
        *out_team = existing;
        return apl_use_case_commit(&transaction, "cannot archive team", error);
    }
    if (status == APL_OK) {
        apl_team *active_teams = nullptr;
        size_t active_count = 0;
        status = apl_team_repository_list(&app->db, false, &active_teams, &active_count, error);
        apl_team_repository_free_list(active_teams);
        if (status == APL_OK && active_count <= 1) {
            apl_error_set(error, APL_ERR_CONFLICT,
                          "the last active team cannot be archived; create another team first");
            status = APL_ERR_CONFLICT;
        }
    }

    int64_t active_team_id = 0;
    if (status == APL_OK) {
        status = apl_settings_repository_get_active_team_id(&app->db, &active_team_id, error);
    }
    apl_team fallback = {};
    bool fallback_found = false;
    if (status == APL_OK) {
        status = apl_team_repository_archive(&app->db, id, out_team, error);
    }
    if (status == APL_OK && active_team_id == id) {
        status = find_oldest_active_team(&app->db, &fallback, &fallback_found, error);
        if (status == APL_OK && fallback_found) {
            status = apl_settings_repository_set_active_team_id(&app->db, fallback.id, error);
        } else if (status == APL_OK) {
            status = apl_settings_repository_clear_active_team_id(&app->db, error);
        }
    }

    if (status == APL_OK) {
        apl_activity_change changes[1] = {};
        set_change_text(&changes[0], "archived", "false", false, "true", false);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        if (active_team_id == id && fallback_found) {
            (void)snprintf(summary, sizeof(summary), "Archived team '%s'; active team is now '%s'",
                           existing.name, fallback.name);
        } else if (active_team_id == id) {
            (void)snprintf(summary, sizeof(summary), "Archived team '%s'; no active team remains",
                           existing.name);
        } else {
            (void)snprintf(summary, sizeof(summary), "Archived team '%s'", existing.name);
        }
        status = append_team_event(app, id, "team_archived", summary, changes, 1, error);
    }

    if (status != APL_OK) {
        const apl_status original = status;
        return apl_use_case_rollback(&transaction, original, "cannot archive team", error);
    }

    return apl_use_case_commit(&transaction, "cannot archive team", error);
}

apl_status apl_team_service_active(const apl_app *const app, apl_team *const out_team,
                                   apl_active_team_status *const out_status, apl_error *const error)
{
    if (app == nullptr || out_team == nullptr || out_status == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "active-team arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }

    *out_team = (apl_team){};
    *out_status = APL_ACTIVE_TEAM_NONE;

    int64_t team_id = 0;
    apl_status status = apl_settings_repository_get_active_team_id(&app->db, &team_id, error);
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot load active team", error);
    }
    if (team_id == 0) {
        return APL_OK;
    }

    status = apl_team_repository_get(&app->db, team_id, out_team, error);
    if (status == APL_OK && !out_team->meta.archived) {
        *out_status = APL_ACTIVE_TEAM_ACTIVE;
        return APL_OK;
    }
    if (status != APL_OK && status != APL_ERR_NOT_FOUND) {
        return apl_use_case_with_context(status, "cannot load active team", error);
    }
    apl_error_clear(error);

    apl_team fallback = {};
    bool found = false;
    status = find_oldest_active_team(&app->db, &fallback, &found, error);
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot load active team", error);
    }
    if (!found) {
        status = apl_settings_repository_clear_active_team_id(&app->db, error);
        if (status != APL_OK) {
            return apl_use_case_with_context(status, "cannot load active team", error);
        }
        *out_team = (apl_team){};
        *out_status = APL_ACTIVE_TEAM_NONE;
        return APL_OK;
    }

    status = apl_settings_repository_set_active_team_id(&app->db, fallback.id, error);
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot load active team", error);
    }
    *out_team = fallback;
    *out_status = APL_ACTIVE_TEAM_FALLBACK;
    return APL_OK;
}

apl_status apl_team_service_set_active(const apl_app *const app, const int64_t id,
                                       apl_error *const error)
{
    if (app == nullptr || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "team switch arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(
        apl_settings_repository_set_active_team_id(&app->db, id, error), "cannot switch team",
        error);
}

apl_status apl_team_service_set_currency(const apl_app *const app, const int64_t id,
                                         const char *const currency, apl_team *const out_team,
                                         apl_error *const error)
{
    if (app == nullptr || out_team == nullptr || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "team currency arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    char normalized[APL_MONEY_CURRENCY_MAX];
    apl_status status = apl_use_case_with_field(
        apl_money_currency_normalize(currency, normalized, sizeof(normalized), error),
        APL_FIELD_CURRENCY, error);
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot set reporting currency", error);
    }

    apl_db_transaction transaction = {};
    status = apl_use_case_begin(&transaction, app, "cannot set reporting currency", error);
    if (status != APL_OK) {
        return status;
    }
    apl_team existing = {};
    status = apl_team_repository_get(&app->db, id, &existing, error);
    if (status == APL_OK && existing.meta.archived) {
        apl_error_set(error, APL_ERR_CONFLICT,
                      "an archived team's reporting currency cannot be changed");
        status = APL_ERR_CONFLICT;
    }
    if (status == APL_OK && strcmp(existing.currency, normalized) == 0) {
        *out_team = existing;
        return apl_use_case_commit(&transaction, "cannot set reporting currency", error);
    }
    if (status == APL_OK && existing.currency[0] != '\0') {
        apl_ledger_entry *entries = nullptr;
        size_t entry_count = 0;
        const apl_ledger_filter ledger_filter = {.team_id = id};
        status =
            apl_ledger_repository_list(&app->db, &ledger_filter, &entries, &entry_count, error);
        apl_ledger_repository_free_list(entries);
        apl_budget_plan *plans = nullptr;
        size_t plan_count = 0;
        if (status == APL_OK) {
            const apl_budget_filter budget_filter = {.team_id = id, .include_archived = true};
            status =
                apl_budget_repository_list(&app->db, &budget_filter, &plans, &plan_count, error);
            apl_budget_repository_free_list(plans);
        }
        if (status == APL_OK && (entry_count > 0 || plan_count > 0)) {
            apl_error_set(error, APL_ERR_CONFLICT,
                          "the reporting currency cannot change after money records exist; "
                          "v1 does not convert posted amounts");
            status = APL_ERR_CONFLICT;
        }
    }
    if (status == APL_OK) {
        status = apl_team_repository_set_currency(&app->db, id, normalized, out_team, error);
    }
    if (status == APL_OK) {
        apl_activity_change change = {};
        apl_audit_change_text(&change, "currency", existing.currency, existing.currency[0] == '\0',
                              out_team->currency, false);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Set reporting currency of team '%s' to %s",
                       out_team->name, out_team->currency);
        status = append_team_event(app, id, "team_currency_updated", summary, &change, 1, error);
    }
    if (status != APL_OK) {
        const apl_status original = status;
        return apl_use_case_rollback(&transaction, original, "cannot set reporting currency",
                                     error);
    }
    return apl_use_case_commit(&transaction, "cannot set reporting currency", error);
}

void apl_team_service_free_list(apl_team *const teams)
{
    apl_team_repository_free_list(teams);
}
