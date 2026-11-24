#include "app/team_service.h"

#include <stdio.h>
#include <string.h>

#include "db/settings_repository.h"
#include "db/team_repository.h"
#include "db/transaction.h"

/** Add use-case context without replacing the underlying failure detail. */
static apl_status with_context(apl_status status, const char *const context, apl_error *const error)
{
    if (status != APL_OK) {
        apl_error_prefix(error, status, context);
    }
    return status;
}

/** Roll back a failed use-case transaction, preserving the original error. */
static void rollback_preserving_error(apl_db_transaction *const transaction,
                                      const apl_status original_status, apl_error *const error)
{
    apl_error rollback_error;
    apl_error_init(&rollback_error);
    if (apl_db_transaction_rollback(transaction, &rollback_error) != APL_OK) {
        char context[160];
        (void)snprintf(context, sizeof(context), "rollback failed: %s",
                       apl_error_message(&rollback_error));
        apl_error_prefix(error, original_status, context);
    }
}

/** Commit a use-case transaction, retrying cleanup if the helper reports an
 *  active transaction after a commit failure. */
static apl_status commit_transaction(apl_db_transaction *const transaction,
                                     const char *const context, apl_error *const error)
{
    const apl_status status = apl_db_transaction_commit(transaction, error);
    if (status != APL_OK && transaction->active) {
        rollback_preserving_error(transaction, status, error);
    }
    return with_context(status, context, error);
}

apl_status apl_team_service_create(apl_app *const app, const char *const name,
                                   const char *const notes, apl_team *const out_team,
                                   apl_error *const error)
{
    if (app == nullptr || out_team == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "team create arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }

    char normalized_name[APL_TEAM_NAME_MAX];
    char normalized_notes[APL_TEAM_NOTES_MAX];
    apl_status status =
        apl_team_name_normalize(name, normalized_name, sizeof(normalized_name), error);
    if (status == APL_OK) {
        status = apl_team_notes_normalize(notes, normalized_notes, sizeof(normalized_notes), error);
    }
    if (status != APL_OK) {
        return with_context(status, "cannot create team", error);
    }

    apl_db_transaction transaction = {};
    status = apl_db_transaction_begin(&transaction, &app->db, error);
    if (status != APL_OK) {
        return with_context(status, "cannot create team", error);
    }

    status =
        apl_team_repository_create(&app->db, normalized_name, normalized_notes, out_team, error);
    if (status == APL_OK) {
        status = apl_settings_repository_set_active_team_id(&app->db, out_team->id, error);
    }

    if (status != APL_OK) {
        const apl_status original = status;
        rollback_preserving_error(&transaction, original, error);
        return with_context(original, "cannot create team", error);
    }

    return commit_transaction(&transaction, "cannot create team", error);
}

apl_status apl_team_service_list(apl_app *const app, const bool include_archived,
                                 apl_team **const out_teams, size_t *const out_count,
                                 apl_error *const error)
{
    if (app == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "application is nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return with_context(
        apl_team_repository_list(&app->db, include_archived, out_teams, out_count, error),
        "cannot list teams", error);
}

apl_status apl_team_service_get(apl_app *const app, const int64_t id, apl_team *const out_team,
                                apl_error *const error)
{
    if (app == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "application is nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return with_context(apl_team_repository_get(&app->db, id, out_team, error), "cannot load team",
                        error);
}

apl_status apl_team_service_update(apl_app *const app, const int64_t id, const char *const name,
                                   const char *const notes, apl_team *const out_team,
                                   apl_error *const error)
{
    if (app == nullptr || out_team == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "team update arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }

    char normalized_name[APL_TEAM_NAME_MAX];
    char normalized_notes[APL_TEAM_NOTES_MAX];
    apl_status status =
        apl_team_name_normalize(name, normalized_name, sizeof(normalized_name), error);
    if (status == APL_OK) {
        status = apl_team_notes_normalize(notes, normalized_notes, sizeof(normalized_notes), error);
    }
    if (status != APL_OK) {
        return with_context(status, "cannot update team", error);
    }

    return with_context(apl_team_repository_update(&app->db, id, normalized_name, normalized_notes,
                                                   out_team, error),
                        "cannot update team", error);
}

/** Choose the oldest-created non-archived team, with stable ID as tie-breaker. */
static apl_status find_oldest_active_team(apl_db_connection *const connection,
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
        if (best == nullptr || strcmp(teams[index].created_at, best->created_at) < 0 ||
            (strcmp(teams[index].created_at, best->created_at) == 0 &&
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

apl_status apl_team_service_archive(apl_app *const app, const int64_t id, apl_team *const out_team,
                                    apl_error *const error)
{
    if (app == nullptr || out_team == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "team archive arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }

    apl_db_transaction transaction = {};
    apl_status status = apl_db_transaction_begin(&transaction, &app->db, error);
    if (status != APL_OK) {
        return with_context(status, "cannot archive team", error);
    }

    apl_team existing = {};
    status = apl_team_repository_get(&app->db, id, &existing, error);
    if (status == APL_OK && !existing.archived) {
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
    if (status == APL_OK) {
        status = apl_team_repository_archive(&app->db, id, out_team, error);
    }
    if (status == APL_OK && active_team_id == id) {
        apl_team fallback = {};
        bool found = false;
        status = find_oldest_active_team(&app->db, &fallback, &found, error);
        if (status == APL_OK && found) {
            status = apl_settings_repository_set_active_team_id(&app->db, fallback.id, error);
        } else if (status == APL_OK) {
            status = apl_settings_repository_clear_active_team_id(&app->db, error);
        }
    }

    if (status != APL_OK) {
        const apl_status original = status;
        rollback_preserving_error(&transaction, original, error);
        return with_context(original, "cannot archive team", error);
    }

    return commit_transaction(&transaction, "cannot archive team", error);
}

apl_status apl_team_service_active(apl_app *const app, apl_team *const out_team,
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
        return with_context(status, "cannot load active team", error);
    }
    if (team_id == 0) {
        return APL_OK;
    }

    status = apl_team_repository_get(&app->db, team_id, out_team, error);
    if (status == APL_OK && !out_team->archived) {
        *out_status = APL_ACTIVE_TEAM_ACTIVE;
        return APL_OK;
    }
    if (status != APL_OK && status != APL_ERR_NOT_FOUND) {
        return with_context(status, "cannot load active team", error);
    }
    apl_error_clear(error);

    apl_team fallback = {};
    bool found = false;
    status = find_oldest_active_team(&app->db, &fallback, &found, error);
    if (status != APL_OK) {
        return with_context(status, "cannot load active team", error);
    }
    if (!found) {
        status = apl_settings_repository_clear_active_team_id(&app->db, error);
        if (status != APL_OK) {
            return with_context(status, "cannot load active team", error);
        }
        *out_team = (apl_team){};
        *out_status = APL_ACTIVE_TEAM_NONE;
        return APL_OK;
    }

    status = apl_settings_repository_set_active_team_id(&app->db, fallback.id, error);
    if (status != APL_OK) {
        return with_context(status, "cannot load active team", error);
    }
    *out_team = fallback;
    *out_status = APL_ACTIVE_TEAM_FALLBACK;
    return APL_OK;
}

apl_status apl_team_service_set_active(apl_app *const app, const int64_t id, apl_error *const error)
{
    if (app == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "application is nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return with_context(apl_settings_repository_set_active_team_id(&app->db, id, error),
                        "cannot switch team", error);
}

void apl_team_service_free_list(apl_team *const teams)
{
    apl_team_repository_free_list(teams);
}
