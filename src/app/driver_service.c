#include "app/driver_service.h"

#include <stdio.h>
#include <string.h>

#include "app/audit.h"
#include "db/driver_repository.h"
#include "db/transaction.h"

static apl_status with_context(const apl_status status, const char *const context,
                               apl_error *const error)
{
    if (status != APL_OK) {
        apl_error_prefix(error, status, context);
    }
    return status;
}

static apl_status rollback(apl_db_transaction *const transaction, const apl_status status,
                           const char *const context, apl_error *const error)
{
    apl_error rollback_error;
    apl_error_init(&rollback_error);
    if (apl_db_transaction_rollback(transaction, &rollback_error) != APL_OK) {
        char detail[160];
        (void)snprintf(detail, sizeof(detail), "rollback failed: %s",
                       apl_error_message(&rollback_error));
        apl_error_prefix(error, status, detail);
    }
    return with_context(status, context, error);
}

static apl_status commit(apl_db_transaction *const transaction, const char *const context,
                         apl_error *const error)
{
    const apl_status status = apl_db_transaction_commit(transaction, error);
    if (status != APL_OK && transaction->active) {
        (void)rollback(transaction, status, context, error);
    }
    return with_context(status, context, error);
}

static apl_status normalize_fields(const char *const name, const char *const short_name,
                                   const char *const notes, char *const out_name,
                                   char *const out_short, char *const out_notes,
                                   apl_error *const error)
{
    apl_status status = apl_reference_label_normalize(name, "driver name", out_name,
                                                      APL_REFERENCE_LABEL_MAX, error);
    if (status == APL_OK) {
        status = apl_reference_optional_normalize(short_name, "driver short name", out_short,
                                                  APL_REFERENCE_SHORT_MAX, error);
    }
    if (status == APL_OK) {
        status = apl_reference_optional_normalize(notes, "driver notes", out_notes,
                                                  APL_REFERENCE_NOTES_MAX, error);
    }
    return status;
}

static apl_status append_event(apl_app *const app, const char *const action,
                               const int64_t driver_id, const char *const summary,
                               const apl_activity_change *const changes, const size_t count,
                               apl_error *const error)
{
    /* The driver directory is a shared identity, not team-owned data. A null
     * team scope keeps it visible through the explicit all-teams history and
     * through the shared-event path in an active-team history view. */
    return apl_audit_append(app, 0, action, "driver", driver_id, summary, changes, count, error);
}

apl_status apl_driver_service_create(apl_app *const app, const char *const name,
                                     const char *const short_name, const char *const notes,
                                     apl_driver *const out_driver, apl_error *const error)
{
    if (app == nullptr || out_driver == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "driver create arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    char normalized_name[APL_REFERENCE_LABEL_MAX];
    char normalized_short[APL_REFERENCE_SHORT_MAX];
    char normalized_notes[APL_REFERENCE_NOTES_MAX];
    apl_status status = normalize_fields(name, short_name, notes, normalized_name, normalized_short,
                                         normalized_notes, error);
    if (status != APL_OK) {
        return with_context(status, "cannot create driver", error);
    }

    apl_db_transaction transaction = {};
    status = apl_db_transaction_begin(&transaction, &app->db, error);
    if (status != APL_OK) {
        return with_context(status, "cannot create driver", error);
    }
    status = apl_driver_repository_create(&app->db, normalized_name, normalized_short,
                                          normalized_notes, out_driver, error);
    if (status == APL_OK) {
        apl_activity_change changes[3] = {};
        apl_audit_change_text(&changes[0], "name", nullptr, true, out_driver->name, false);
        apl_audit_change_text(&changes[1], "short_name", nullptr, true, out_driver->short_name,
                              false);
        apl_audit_change_text(&changes[2], "notes", nullptr, true, out_driver->notes, false);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Created driver '%s'", out_driver->name);
        status = append_event(app, "driver_created", out_driver->id, summary, changes, 3, error);
    }
    if (status != APL_OK) {
        return rollback(&transaction, status, "cannot create driver", error);
    }
    return commit(&transaction, "cannot create driver", error);
}

apl_status apl_driver_service_get(apl_app *const app, const int64_t id,
                                  apl_driver *const out_driver, apl_error *const error)
{
    if (app == nullptr || out_driver == nullptr || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "driver get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return with_context(apl_driver_repository_get(&app->db, id, out_driver, error),
                        "cannot load driver", error);
}

apl_status apl_driver_service_update(apl_app *const app, const int64_t id, const char *const name,
                                     const char *const short_name, const char *const notes,
                                     apl_driver *const out_driver, apl_error *const error)
{
    if (app == nullptr || out_driver == nullptr || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "driver update arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    char normalized_name[APL_REFERENCE_LABEL_MAX];
    char normalized_short[APL_REFERENCE_SHORT_MAX];
    char normalized_notes[APL_REFERENCE_NOTES_MAX];
    apl_status status = normalize_fields(name, short_name, notes, normalized_name, normalized_short,
                                         normalized_notes, error);
    if (status != APL_OK) {
        return with_context(status, "cannot update driver", error);
    }

    apl_db_transaction transaction = {};
    status = apl_db_transaction_begin(&transaction, &app->db, error);
    if (status != APL_OK) {
        return with_context(status, "cannot update driver", error);
    }
    apl_driver existing = {};
    status = apl_driver_repository_get(&app->db, id, &existing, error);
    if (status != APL_OK) {
        return rollback(&transaction, status, "cannot update driver", error);
    }
    const bool name_changed = strcmp(existing.name, normalized_name) != 0;
    const bool short_changed = strcmp(existing.short_name, normalized_short) != 0;
    const bool notes_changed = strcmp(existing.notes, normalized_notes) != 0;
    if (!name_changed && !short_changed && !notes_changed) {
        *out_driver = existing;
        return commit(&transaction, "cannot update driver", error);
    }
    status = apl_driver_repository_update(&app->db, id, normalized_name, normalized_short,
                                          normalized_notes, out_driver, error);
    if (status == APL_OK) {
        apl_activity_change changes[3] = {};
        size_t count = 0;
        if (name_changed) {
            apl_audit_change_text(&changes[count++], "name", existing.name, false, out_driver->name,
                                  false);
        }
        if (short_changed) {
            apl_audit_change_text(&changes[count++], "short_name", existing.short_name, false,
                                  out_driver->short_name, false);
        }
        if (notes_changed) {
            apl_audit_change_text(&changes[count++], "notes", existing.notes, false,
                                  out_driver->notes, false);
        }
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Updated driver '%s'", out_driver->name);
        status = append_event(app, "driver_updated", id, summary, changes, count, error);
    }
    if (status != APL_OK) {
        return rollback(&transaction, status, "cannot update driver", error);
    }
    return commit(&transaction, "cannot update driver", error);
}

apl_status apl_driver_service_archive(apl_app *const app, const int64_t id,
                                      apl_driver *const out_driver, apl_error *const error)
{
    if (app == nullptr || out_driver == nullptr || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "driver archive arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_db_transaction transaction = {};
    apl_status status = apl_db_transaction_begin(&transaction, &app->db, error);
    if (status != APL_OK) {
        return with_context(status, "cannot archive driver", error);
    }
    apl_driver existing = {};
    status = apl_driver_repository_get(&app->db, id, &existing, error);
    if (status == APL_OK && existing.archived) {
        *out_driver = existing;
        return commit(&transaction, "cannot archive driver", error);
    }
    if (status == APL_OK) {
        status = apl_driver_repository_archive(&app->db, id, out_driver, error);
    }
    if (status == APL_OK) {
        apl_activity_change change = {};
        apl_audit_change_bool(&change, "archived", false, true);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Archived driver '%s'", out_driver->name);
        status = append_event(app, "driver_archived", id, summary, &change, 1, error);
    }
    if (status != APL_OK) {
        return rollback(&transaction, status, "cannot archive driver", error);
    }
    return commit(&transaction, "cannot archive driver", error);
}

apl_status apl_driver_service_list(apl_app *const app, const bool include_archived,
                                   apl_driver **const out_drivers, size_t *const out_count,
                                   apl_error *const error)
{
    if (app == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "application is nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return with_context(
        apl_driver_repository_list(&app->db, include_archived, out_drivers, out_count, error),
        "cannot list drivers", error);
}

apl_status apl_driver_service_list_for_team(apl_app *const app, const int64_t team_id,
                                            const bool include_archived,
                                            apl_driver **const out_drivers, size_t *const out_count,
                                            apl_error *const error)
{
    if (app == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "team driver list arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return with_context(apl_driver_repository_list_for_team(&app->db, team_id, include_archived,
                                                            out_drivers, out_count, error),
                        "cannot list team drivers", error);
}

void apl_driver_service_free_list(apl_driver *const drivers)
{
    apl_driver_repository_free_list(drivers);
}
