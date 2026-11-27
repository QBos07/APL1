#include "app/lifecycle_service.h"

#include <stdio.h>
#include <string.h>

#include "app/audit.h"
#include "app/use_case.h"
#include "core/util.h"
#include "db/car_repository.h"
#include "db/lifecycle_repository.h"
#include "db/ledger_repository.h"
#include "db/part_unit_repository.h"
#include "db/team_repository.h"
#include "db/transaction.h"
#include "domain/record.h"

static int64_t find_current_fitment(const apl_app *const app, const int64_t team_id,
                                    const int64_t car_id, const char *const position,
                                    apl_error *const error)
{
    apl_fitment_row *rows = nullptr;
    size_t count = 0;
    apl_status status =
        apl_lifecycle_repository_current_fitment(&app->db, team_id, &rows, &count, error);
    if (status != APL_OK) {
        return -1;
    }
    int64_t unit_id = 0;
    for (size_t index = 0; index < count; ++index) {
        if (rows[index].car_id == car_id && strcmp(rows[index].position, position) == 0) {
            unit_id = rows[index].unit_id;
            break;
        }
    }
    apl_lifecycle_repository_free_fitment(rows);
    return unit_id;
}

static apl_status resolve_unit(const apl_app *const app, const int64_t team_id,
                               const int64_t unit_id, apl_part_unit *const out_unit,
                               apl_error *const error)
{
    if (unit_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "a lifecycle action requires a part unit");
        apl_error_set_field(error, APL_FIELD_NEW_UNIT);
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_status status = apl_part_unit_repository_get(&app->db, team_id, unit_id, out_unit, error);
    if (status != APL_OK) {
        apl_error_set_field(error, APL_FIELD_NEW_UNIT);
        return status;
    }
    return APL_OK;
}

static apl_status validate_car(const apl_app *const app, const int64_t team_id,
                               const int64_t car_id, const char *const position,
                               apl_error *const error)
{
    if (car_id <= 0 || position[0] == '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "install/remove require an explicit car and position");
        apl_error_set_field(error, APL_FIELD_POSITION);
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_car car = {};
    const apl_status status = apl_car_repository_get(&app->db, team_id, car_id, &car, error);
    if (status != APL_OK) {
        apl_error_set_field(error, APL_FIELD_CAR);
        return status;
    }
    return APL_OK;
}

static apl_status normalize_fields(const char *const event_date, const char *const event_label,
                                   const char *const reason, const char *const usage,
                                   const char *const notes, apl_lifecycle_event *const event,
                                   apl_error *const error)
{
    apl_status result = apl_lifecycle_date_normalize(event_date, event->event_date,
                                                     sizeof(event->event_date), error);
    if (result == APL_OK) {
        result = apl_lifecycle_event_label_normalize(event_label, event->event_label,
                                                     sizeof(event->event_label), error);
    }
    if (result == APL_OK) {
        result =
            apl_lifecycle_reason_normalize(reason, event->reason, sizeof(event->reason), error);
    }
    if (result == APL_OK) {
        result = apl_lifecycle_usage_normalize(usage, event->usage, sizeof(event->usage), error);
    }
    if (result == APL_OK) {
        result = apl_lifecycle_notes_normalize(notes, event->notes, sizeof(event->notes), error);
    }
    return result;
}

static apl_status append_event(const apl_app *const app, const int64_t team_id,
                               const char *const action, const apl_lifecycle_event *const out,
                               const apl_activity_change *const changes, const size_t count,
                               apl_error *const error)
{
    char summary[APL_ACTIVITY_SUMMARY_MAX];
    (void)snprintf(summary, sizeof(summary), "%s %s on %s", apl_lifecycle_action_label(out->action),
                   out->position[0] != '\0' ? out->position : "unit", out->event_date);
    return apl_audit_append(app, team_id, action, apl_record_kind_code(APL_RECORD_LIFECYCLE_EVENT),
                            out->id, summary, changes, count, error);
}

static apl_status record_simple(const apl_app *const app, const int64_t team_id,
                                const apl_lifecycle_action action, const int64_t unit_id,
                                const int64_t car_id, const char *const position,
                                const char *const event_date, const char *const event_label,
                                const char *const reason, const char *const usage,
                                const char *const notes, apl_lifecycle_event *const out_event,
                                const char *const context, const char *const action_code,
                                apl_error *const error)
{
    apl_lifecycle_event event = {
        .team_id = team_id, .action = action, .unit_id = unit_id, .car_id = car_id};
    (void)snprintf(event.position, sizeof(event.position), "%s", position);
    apl_status result =
        normalize_fields(event_date, event_label, reason, usage, notes, &event, error);
    if (result == APL_OK) {
        result = resolve_unit(app, team_id, unit_id, &(apl_part_unit){}, error);
    }
    if (result == APL_OK && (action == APL_LIFECYCLE_INSTALL || action == APL_LIFECYCLE_REMOVE)) {
        result = validate_car(app, team_id, car_id, position, error);
    }
    if (result != APL_OK) {
        return apl_use_case_with_context(result, context, error);
    }

    apl_db_transaction transaction = {};
    result = apl_use_case_begin(&transaction, app, context, error);
    if (result != APL_OK) {
        return result;
    }

    if (action == APL_LIFECYCLE_INSTALL) {
        const int64_t existing = find_current_fitment(app, team_id, car_id, position, error);
        if (existing < 0) {
            return apl_use_case_rollback(&transaction, APL_ERR_DB_QUERY, context, error);
        }
        if (existing != 0) {
            apl_error_set(error, APL_ERR_CONFLICT,
                          "a unit is already fitted at this car/position; use replace or remove "
                          "it first");
            apl_error_set_field(error, APL_FIELD_POSITION);
            return apl_use_case_rollback(&transaction, APL_ERR_CONFLICT, context, error);
        }
    } else if (action == APL_LIFECYCLE_REMOVE) {
        const int64_t existing = find_current_fitment(app, team_id, car_id, position, error);
        if (existing < 0) {
            return apl_use_case_rollback(&transaction, APL_ERR_DB_QUERY, context, error);
        }
        if (existing == 0 || existing != unit_id) {
            apl_error_set(error, APL_ERR_CONFLICT,
                          "the requested unit is not the currently-recorded fitment at this "
                          "car/position");
            apl_error_set_field(error, APL_FIELD_POSITION);
            return apl_use_case_rollback(&transaction, APL_ERR_CONFLICT, context, error);
        }
    }

    result = apl_lifecycle_repository_append(&app->db, team_id, &event, out_event, error);
    if (result == APL_OK) {
        apl_activity_change changes[4] = {};
        char unit_text[32] = "";
        (void)snprintf(unit_text, sizeof(unit_text), "%lld", (long long)out_event->unit_id);
        apl_audit_change_text(&changes[0], "unit", nullptr, true, unit_text, false);
        apl_audit_change_text(&changes[1], "car", nullptr, true,
                              (out_event->car_id > 0) ? "linked" : "", out_event->car_id == 0);
        apl_audit_change_text(&changes[2], "date", nullptr, true, out_event->event_date, false);
        apl_audit_change_text(&changes[3], "notes", nullptr, true, out_event->notes,
                              out_event->notes[0] == '\0');
        result = append_event(app, team_id, action_code, out_event, changes, APL_COUNT_OF(changes),
                              error);
    }
    if (result != APL_OK) {
        return apl_use_case_rollback(&transaction, result, context, error);
    }
    return apl_use_case_commit(&transaction, context, error);
}

apl_status apl_lifecycle_service_install(const apl_app *const app, const int64_t team_id,
                                         const int64_t unit_id, const int64_t car_id,
                                         const char *const position, const char *const event_date,
                                         const char *const event_label, const char *const reason,
                                         const char *const usage, const char *const notes,
                                         apl_lifecycle_event *const out_event,
                                         apl_error *const error)
{
    return record_simple(app, team_id, APL_LIFECYCLE_INSTALL, unit_id, car_id, position, event_date,
                         event_label, reason, usage, notes, out_event, "cannot record install",
                         "lifecycle_installed", error);
}

apl_status apl_lifecycle_service_remove(const apl_app *const app, const int64_t team_id,
                                        const int64_t unit_id, const int64_t car_id,
                                        const char *const position, const char *const event_date,
                                        const char *const event_label, const char *const reason,
                                        const char *const usage, const char *const notes,
                                        apl_lifecycle_event *const out_event,
                                        apl_error *const error)
{
    return record_simple(app, team_id, APL_LIFECYCLE_REMOVE, unit_id, car_id, position, event_date,
                         event_label, reason, usage, notes, out_event, "cannot record removal",
                         "lifecycle_removed", error);
}

apl_status apl_lifecycle_service_repair(const apl_app *const app, const int64_t team_id,
                                        const int64_t unit_id, const int64_t car_id,
                                        const char *const position, const char *const event_date,
                                        const char *const event_label, const char *const reason,
                                        const char *const usage, const char *const notes,
                                        apl_lifecycle_event *const out_event,
                                        apl_error *const error)
{
    return record_simple(app, team_id, APL_LIFECYCLE_REPAIR, unit_id, car_id, position, event_date,
                         event_label, reason, usage, notes, out_event, "cannot record repair",
                         "lifecycle_repaired", error);
}

apl_status apl_lifecycle_service_dispose(const apl_app *const app, const int64_t team_id,
                                         const int64_t unit_id, const char *const event_date,
                                         const char *const event_label, const char *const reason,
                                         const char *const usage, const char *const notes,
                                         apl_lifecycle_event *const out_event,
                                         apl_error *const error)
{
    apl_lifecycle_event event = {
        .team_id = team_id, .action = APL_LIFECYCLE_DISPOSE, .unit_id = unit_id};
    apl_status result =
        normalize_fields(event_date, event_label, reason, usage, notes, &event, error);
    if (result == APL_OK) {
        result = resolve_unit(app, team_id, unit_id, &(apl_part_unit){}, error);
    }
    if (result != APL_OK) {
        return apl_use_case_with_context(result, "cannot record dispose", error);
    }

    apl_db_transaction transaction = {};
    result = apl_use_case_begin(&transaction, app, "cannot record dispose", error);
    if (result != APL_OK) {
        return result;
    }
    /* A disposed unit must not be currently fitted anywhere. */
    apl_fitment_row *rows = nullptr;
    size_t count = 0;
    result = apl_lifecycle_repository_current_fitment(&app->db, team_id, &rows, &count, error);
    if (result == APL_OK) {
        for (size_t index = 0; index < count; ++index) {
            if (rows[index].unit_id == unit_id) {
                result = APL_ERR_CONFLICT;
                apl_error_set(error, APL_ERR_CONFLICT,
                              "a currently-fitted unit must be removed before it can be disposed");
                apl_error_set_field(error, APL_FIELD_NEW_UNIT);
                break;
            }
        }
        apl_lifecycle_repository_free_fitment(rows);
    }
    if (result != APL_OK) {
        return apl_use_case_rollback(&transaction, result, "cannot record dispose", error);
    }

    result = apl_lifecycle_repository_append(&app->db, team_id, &event, out_event, error);
    if (result == APL_OK) {
        apl_activity_change changes[4] = {};
        apl_audit_change_text(&changes[0], "unit_id", nullptr, true, "referenced", false);
        apl_audit_change_text(&changes[1], "date", nullptr, true, out_event->event_date, false);
        apl_audit_change_text(&changes[2], "reason", nullptr, true, out_event->reason,
                              out_event->reason[0] == '\0');
        apl_audit_change_text(&changes[3], "notes", nullptr, true, out_event->notes,
                              out_event->notes[0] == '\0');
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Disposed unit %lld on %s",
                       (long long)out_event->unit_id, out_event->event_date);
        result = apl_audit_append(app, team_id, "lifecycle_disposed",
                                  apl_record_kind_code(APL_RECORD_LIFECYCLE_EVENT), out_event->id,
                                  summary, changes, 4, error);
    }
    if (result != APL_OK) {
        return apl_use_case_rollback(&transaction, result, "cannot record dispose", error);
    }
    return apl_use_case_commit(&transaction, "cannot record dispose", error);
}

apl_status apl_lifecycle_service_replace(const apl_app *const app, const int64_t team_id,
                                         const int64_t car_id, const char *const position,
                                         const int64_t old_unit_id, const int64_t new_unit_id,
                                         const char *const event_date,
                                         const char *const event_label, const char *const reason,
                                         const char *const usage, const char *const notes,
                                         apl_lifecycle_event *const out_event,
                                         apl_error *const error)
{
    apl_lifecycle_event event = {.team_id = team_id,
                                 .action = APL_LIFECYCLE_REPLACE,
                                 .car_id = car_id,
                                 .old_unit_id = old_unit_id,
                                 .new_unit_id = new_unit_id};
    (void)snprintf(event.position, sizeof(event.position), "%s", position);
    apl_status status =
        normalize_fields(event_date, event_label, reason, usage, notes, &event, error);
    if (status == APL_OK) {
        status = resolve_unit(app, team_id, old_unit_id, &(apl_part_unit){}, error);
    }
    if (status == APL_OK) {
        status = resolve_unit(app, team_id, new_unit_id, &(apl_part_unit){}, error);
    }
    if (status == APL_OK) {
        status = validate_car(app, team_id, car_id, position, error);
    }
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot record replacement", error);
    }

    apl_db_transaction transaction = {};
    status = apl_use_case_begin(&transaction, app, "cannot record replacement", error);
    if (status != APL_OK) {
        return status;
    }

    /* The old unit must be the currently-recorded fitment here. */
    const int64_t current = find_current_fitment(app, team_id, car_id, position, error);
    if (current < 0) {
        return apl_use_case_rollback(&transaction, APL_ERR_DB_QUERY, "cannot record replacement",
                                     error);
    }
    if (current == 0 || current != old_unit_id) {
        apl_error_set(error, APL_ERR_CONFLICT,
                      "the selected old unit is not the currently-recorded fitment at this "
                      "car/position");
        apl_error_set_field(error, APL_FIELD_OLD_UNIT);
        return apl_use_case_rollback(&transaction, APL_ERR_CONFLICT, "cannot record replacement",
                                     error);
    }
    if (old_unit_id == new_unit_id) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "the old and new units must be different for a replacement");
        apl_error_set_field(error, APL_FIELD_NEW_UNIT);
        return apl_use_case_rollback(&transaction, APL_ERR_INVALID_ARGUMENT,
                                     "cannot record replacement", error);
    }
    /* The new unit must not be fitted elsewhere in this team. */
    apl_fitment_row *rows = nullptr;
    size_t count = 0;
    status = apl_lifecycle_repository_current_fitment(&app->db, team_id, &rows, &count, error);
    if (status == APL_OK) {
        for (size_t index = 0; index < count && status == APL_OK; ++index) {
            if (rows[index].unit_id == new_unit_id) {
                apl_error_set(error, APL_ERR_CONFLICT,
                              "the new unit is already fitted elsewhere; remove it first");
                apl_error_set_field(error, APL_FIELD_NEW_UNIT);
                status = APL_ERR_CONFLICT;
            }
        }
        apl_lifecycle_repository_free_fitment(rows);
    }
    if (status != APL_OK) {
        return apl_use_case_rollback(&transaction, status, "cannot record replacement", error);
    }

    status = apl_lifecycle_repository_append(&app->db, team_id, &event, out_event, error);
    if (status == APL_OK) {
        apl_activity_change changes[4] = {};
        char old_text[32];
        char new_text[32];
        (void)snprintf(old_text, sizeof(old_text), "%lld", (long long)old_unit_id);
        (void)snprintf(new_text, sizeof(new_text), "%lld", (long long)new_unit_id);
        apl_audit_change_text(&changes[0], "old_unit", nullptr, true, old_text, false);
        apl_audit_change_text(&changes[1], "new_unit", nullptr, true, new_text, false);
        apl_audit_change_text(&changes[2], "date", nullptr, true, out_event->event_date, false);
        apl_audit_change_text(&changes[3], "reason", nullptr, true, out_event->reason,
                              out_event->reason[0] == '\0');
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Replaced unit %lld with %lld on %s",
                       (long long)old_unit_id, (long long)new_unit_id, out_event->event_date);
        status = apl_audit_append(app, team_id, "lifecycle_replaced",
                                  apl_record_kind_code(APL_RECORD_LIFECYCLE_EVENT), out_event->id,
                                  summary, changes, 4, error);
    }
    if (status != APL_OK) {
        return apl_use_case_rollback(&transaction, status, "cannot record replacement", error);
    }
    return apl_use_case_commit(&transaction, "cannot record replacement", error);
}

apl_status apl_lifecycle_service_set_cost(const apl_app *const app, const int64_t team_id,
                                          const int64_t event_id, const int64_t transaction_id,
                                          apl_lifecycle_event *const out_event,
                                          apl_error *const error)
{
    if (app == nullptr || out_event == nullptr || team_id <= 0 || event_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "replacement cost-link arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_event = (apl_lifecycle_event){};
    if (transaction_id < 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "replacement cost link must be a positive expense ID or empty");
        apl_error_set_field(error, APL_FIELD_TRANSACTION);
        return APL_ERR_INVALID_ARGUMENT;
    }

    apl_status status = APL_OK;
    if (transaction_id > 0) {
        apl_ledger_entry entry = {};
        status = apl_ledger_repository_get(&app->db, team_id, transaction_id, &entry, error);
        if (status != APL_OK) {
            apl_error_set_field(error, APL_FIELD_TRANSACTION);
            return apl_use_case_with_context(status, "cannot link replacement cost", error);
        }
        if (entry.classification != APL_LEDGER_KIND_EXPENSE) {
            apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                          "only an expense transaction can be linked as a replacement cost");
            apl_error_set_field(error, APL_FIELD_TRANSACTION);
            return apl_use_case_with_context(APL_ERR_INVALID_ARGUMENT,
                                             "cannot link replacement cost", error);
        }
        apl_team team = {};
        status = apl_team_repository_get(&app->db, team_id, &team, error);
        if (status == APL_OK && team.currency[0] != '\0' &&
            strcmp(team.currency, entry.amount.currency) != 0) {
            apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                          "the linked expense transaction uses a different currency than the team");
            apl_error_set_field(error, APL_FIELD_TRANSACTION);
            status = APL_ERR_INVALID_ARGUMENT;
        }
        if (status != APL_OK) {
            return apl_use_case_with_context(status, "cannot link replacement cost", error);
        }
    }

    apl_db_transaction transaction = {};
    status = apl_use_case_begin(&transaction, app, "cannot link replacement cost", error);
    if (status != APL_OK) {
        return status;
    }
    status = apl_lifecycle_repository_set_transaction(&app->db, team_id, event_id, transaction_id,
                                                      out_event, error);
    if (status == APL_OK) {
        apl_activity_change change = {};
        const char *new_text = (out_event->transaction_id > 0) ? "linked" : "";
        apl_audit_change_text(&change, "cost_link", nullptr, true, new_text,
                              out_event->transaction_id == 0);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        if (out_event->transaction_id > 0) {
            (void)snprintf(summary, sizeof(summary),
                           "Linked replacement to expense transaction %lld",
                           (long long)out_event->transaction_id);
        } else {
            (void)snprintf(summary, sizeof(summary), "Removed replacement cost link");
        }
        status = apl_audit_append(app, team_id, "lifecycle_cost_linked",
                                  apl_record_kind_code(APL_RECORD_LIFECYCLE_EVENT), out_event->id,
                                  summary, &change, 1, error);
    }
    if (status != APL_OK) {
        return apl_use_case_rollback(&transaction, status, "cannot link replacement cost", error);
    }
    return apl_use_case_commit(&transaction, "cannot link replacement cost", error);
}

apl_status apl_lifecycle_service_get(const apl_app *const app, const int64_t team_id,
                                     const int64_t id, apl_lifecycle_event *const out_event,
                                     apl_error *const error)
{
    if (app == nullptr || out_event == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "lifecycle get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(
        apl_lifecycle_repository_get(&app->db, team_id, id, out_event, error),
        "cannot load lifecycle event", error);
}

apl_status apl_lifecycle_service_list(const apl_app *const app,
                                      const apl_lifecycle_filter *const filter,
                                      apl_lifecycle_event **const out_events,
                                      size_t *const out_count, apl_error *const error)
{
    if (app == nullptr || filter == nullptr || filter->team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "lifecycle list arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(
        apl_lifecycle_repository_list(&app->db, filter, out_events, out_count, error),
        "cannot list lifecycle events", error);
}

apl_status apl_lifecycle_service_list_unit(const apl_app *const app, const int64_t team_id,
                                           const int64_t unit_id,
                                           apl_lifecycle_history_entry **const out_entries,
                                           size_t *const out_count, apl_error *const error)
{
    if (app == nullptr || out_entries == nullptr || out_count == nullptr || team_id <= 0 ||
        unit_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "lifecycle unit list arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(apl_lifecycle_repository_list_unit(
                                         &app->db, team_id, unit_id, out_entries, out_count, error),
                                     "cannot list unit lifecycle", error);
}

apl_status apl_lifecycle_service_current_fitment(const apl_app *const app, const int64_t team_id,
                                                 apl_fitment_row **const out_rows,
                                                 size_t *const out_count, apl_error *const error)
{
    if (app == nullptr || out_rows == nullptr || out_count == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "current fitment arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(
        apl_lifecycle_repository_current_fitment(&app->db, team_id, out_rows, out_count, error),
        "cannot read current fitment", error);
}

void apl_lifecycle_service_free_events(apl_lifecycle_event *const events)
{
    apl_lifecycle_repository_free_events(events);
}

void apl_lifecycle_service_free_history(apl_lifecycle_history_entry *const entries)
{
    apl_lifecycle_repository_free_history(entries);
}

void apl_lifecycle_service_free_fitment(apl_fitment_row *const rows)
{
    apl_lifecycle_repository_free_fitment(rows);
}