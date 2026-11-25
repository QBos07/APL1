#include "app/car_service.h"

#include <stdio.h>
#include <string.h>

#include "app/audit.h"
#include "db/car_repository.h"
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

static apl_status normalize_fields(const char *const label, const char *const chassis,
                                   const char *const notes, char *const out_label,
                                   char *const out_chassis, char *const out_notes,
                                   apl_error *const error)
{
    apl_status status = apl_reference_label_normalize(label, "car label", out_label,
                                                      APL_REFERENCE_LABEL_MAX, error);
    if (status == APL_OK) {
        status = apl_reference_optional_normalize(chassis, "car chassis", out_chassis,
                                                  APL_REFERENCE_SHORT_MAX, error);
    }
    if (status == APL_OK) {
        status = apl_reference_optional_normalize(notes, "car notes", out_notes,
                                                  APL_REFERENCE_NOTES_MAX, error);
    }
    return status;
}

static apl_status append_event(const apl_app *const app, const int64_t team_id,
                               const char *const action, const int64_t car_id,
                               const char *const summary, const apl_activity_change *const changes,
                               const size_t count, apl_error *const error)
{
    return apl_audit_append(app, team_id, action, "car", car_id, summary, changes, count, error);
}

apl_status apl_car_service_create(const apl_app *const app, const int64_t team_id,
                                  const char *const label, const char *const chassis,
                                  const char *const notes, apl_car *const out_car,
                                  apl_error *const error)
{
    if (app == nullptr || out_car == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "car create arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    char normalized_label[APL_REFERENCE_LABEL_MAX];
    char normalized_chassis[APL_REFERENCE_SHORT_MAX];
    char normalized_notes[APL_REFERENCE_NOTES_MAX];
    apl_status status = normalize_fields(label, chassis, notes, normalized_label,
                                         normalized_chassis, normalized_notes, error);
    if (status != APL_OK) {
        return with_context(status, "cannot create car", error);
    }

    apl_db_transaction transaction = {};
    status = apl_db_transaction_begin(&transaction, &app->db, error);
    if (status != APL_OK) {
        return with_context(status, "cannot create car", error);
    }
    status = apl_car_repository_create(&app->db, team_id, normalized_label, normalized_chassis,
                                       normalized_notes, out_car, error);
    if (status == APL_OK) {
        apl_activity_change changes[3] = {};
        apl_audit_change_text(&changes[0], "label", nullptr, true, out_car->label, false);
        apl_audit_change_text(&changes[1], "chassis", nullptr, true, out_car->chassis, false);
        apl_audit_change_text(&changes[2], "notes", nullptr, true, out_car->notes, false);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Created car '%s'", out_car->label);
        status = append_event(app, team_id, "car_created", out_car->id, summary, changes, 3, error);
    }
    if (status != APL_OK) {
        return rollback(&transaction, status, "cannot create car", error);
    }
    return commit(&transaction, "cannot create car", error);
}

apl_status apl_car_service_get(const apl_app *const app, const int64_t team_id, const int64_t id,
                               apl_car *const out_car, apl_error *const error)
{
    if (app == nullptr || out_car == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "car get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return with_context(apl_car_repository_get(&app->db, team_id, id, out_car, error),
                        "cannot load car", error);
}

apl_status apl_car_service_update(const apl_app *const app, const int64_t team_id, const int64_t id,
                                  const char *const label, const char *const chassis,
                                  const char *const notes, apl_car *const out_car,
                                  apl_error *const error)
{
    if (app == nullptr || out_car == nullptr || team_id <= 0 || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "car update arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    char normalized_label[APL_REFERENCE_LABEL_MAX];
    char normalized_chassis[APL_REFERENCE_SHORT_MAX];
    char normalized_notes[APL_REFERENCE_NOTES_MAX];
    apl_status status = normalize_fields(label, chassis, notes, normalized_label,
                                         normalized_chassis, normalized_notes, error);
    if (status != APL_OK) {
        return with_context(status, "cannot update car", error);
    }

    apl_db_transaction transaction = {};
    status = apl_db_transaction_begin(&transaction, &app->db, error);
    if (status != APL_OK) {
        return with_context(status, "cannot update car", error);
    }
    apl_car existing = {};
    status = apl_car_repository_get(&app->db, team_id, id, &existing, error);
    if (status != APL_OK) {
        return rollback(&transaction, status, "cannot update car", error);
    }
    const bool label_changed = strcmp(existing.label, normalized_label) != 0;
    const bool chassis_changed = strcmp(existing.chassis, normalized_chassis) != 0;
    const bool notes_changed = strcmp(existing.notes, normalized_notes) != 0;
    if (!label_changed && !chassis_changed && !notes_changed) {
        *out_car = existing;
        return commit(&transaction, "cannot update car", error);
    }
    status = apl_car_repository_update(&app->db, team_id, id, normalized_label, normalized_chassis,
                                       normalized_notes, out_car, error);
    if (status == APL_OK) {
        apl_activity_change changes[3] = {};
        size_t count = 0;
        if (label_changed) {
            apl_audit_change_text(&changes[count++], "label", existing.label, false, out_car->label,
                                  false);
        }
        if (chassis_changed) {
            apl_audit_change_text(&changes[count++], "chassis", existing.chassis, false,
                                  out_car->chassis, false);
        }
        if (notes_changed) {
            apl_audit_change_text(&changes[count++], "notes", existing.notes, false, out_car->notes,
                                  false);
        }
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Updated car '%s'", out_car->label);
        status = append_event(app, team_id, "car_updated", id, summary, changes, count, error);
    }
    if (status != APL_OK) {
        return rollback(&transaction, status, "cannot update car", error);
    }
    return commit(&transaction, "cannot update car", error);
}

apl_status apl_car_service_archive(const apl_app *const app, const int64_t team_id,
                                   const int64_t id, apl_car *const out_car, apl_error *const error)
{
    if (app == nullptr || out_car == nullptr || team_id <= 0 || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "car archive arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_db_transaction transaction = {};
    apl_status status = apl_db_transaction_begin(&transaction, &app->db, error);
    if (status != APL_OK) {
        return with_context(status, "cannot archive car", error);
    }
    apl_car existing = {};
    status = apl_car_repository_get(&app->db, team_id, id, &existing, error);
    if (status == APL_OK && existing.archived) {
        *out_car = existing;
        return commit(&transaction, "cannot archive car", error);
    }
    if (status == APL_OK) {
        status = apl_car_repository_archive(&app->db, team_id, id, out_car, error);
    }
    if (status == APL_OK) {
        apl_activity_change change = {};
        apl_audit_change_bool(&change, "archived", false, true);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Archived car '%s'", out_car->label);
        status = append_event(app, team_id, "car_archived", id, summary, &change, 1, error);
    }
    if (status != APL_OK) {
        return rollback(&transaction, status, "cannot archive car", error);
    }
    return commit(&transaction, "cannot archive car", error);
}

apl_status apl_car_service_list(const apl_app *const app, const int64_t team_id,
                                const bool include_archived, apl_car **const out_cars,
                                size_t *const out_count, apl_error *const error)
{
    if (app == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "car list arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return with_context(
        apl_car_repository_list(&app->db, team_id, include_archived, out_cars, out_count, error),
        "cannot list cars", error);
}

void apl_car_service_free_list(apl_car *const cars)
{
    apl_car_repository_free_list(cars);
}
