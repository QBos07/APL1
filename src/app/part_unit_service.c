#include "app/part_unit_service.h"

#include <stdio.h>
#include <string.h>

#include "app/audit.h"
#include "app/use_case.h"
#include "core/util.h"
#include "db/part_type_repository.h"
#include "db/part_unit_repository.h"
#include "db/transaction.h"
#include "domain/record.h"

static apl_status normalize_fields(const char *const serial_number, const char *const part_number,
                                   const char *const condition, const char *const notes,
                                   apl_part_unit *const unit, apl_error *const error)
{
    apl_status result = apl_part_unit_serial_normalize(serial_number, unit->serial_number,
                                                       sizeof(unit->serial_number), error);
    if (result == APL_OK) {
        result = apl_part_unit_number_normalize(part_number, unit->part_number,
                                                sizeof(unit->part_number), error);
    }
    if (result == APL_OK) {
        result = apl_part_unit_condition_normalize(condition, "part condition", unit->condition,
                                                   sizeof(unit->condition), unit->condition_key,
                                                   sizeof(unit->condition_key), error);
    }
    if (result == APL_OK) {
        result = apl_part_unit_notes_normalize(notes, unit->notes, sizeof(unit->notes), error);
    }
    return result;
}

static apl_status validate_type(const apl_app *const app, const int64_t team_id,
                                const int64_t type_id, apl_error *const error)
{
    if (type_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "a part unit requires a same-team part type");
        apl_error_set_field(error, APL_FIELD_CATEGORY);
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_part_type type = {};
    const apl_status status =
        apl_part_type_repository_get(&app->db, team_id, type_id, &type, error);
    if (status != APL_OK) {
        apl_error_set_field(error, APL_FIELD_CATEGORY);
        return status;
    }
    return APL_OK;
}

static void change_id(apl_activity_change *const change, const char *const field,
                      const int64_t value)
{
    char text[32];
    (void)snprintf(text, sizeof(text), "%lld", (long long)value);
    apl_audit_change_text(change, field, nullptr, true, text, false);
}

static apl_status append_event(const apl_app *const app, const int64_t team_id,
                               const char *const action, const int64_t unit_id,
                               const char *const summary, const apl_activity_change *const changes,
                               const size_t count, apl_error *const error)
{
    return apl_audit_append(app, team_id, action, apl_record_kind_code(APL_RECORD_PART_UNIT),
                            unit_id, summary, changes, count, error);
}

apl_status apl_part_unit_service_create(const apl_app *const app, const int64_t team_id,
                                        const int64_t type_id, const char *const serial_number,
                                        const char *const part_number, const char *const condition,
                                        const char *const notes, apl_part_unit *const out_unit,
                                        apl_error *const error)
{
    if (app == nullptr || out_unit == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part unit create arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_unit = (apl_part_unit){};

    apl_part_unit unit = {.team_id = team_id, .type_id = type_id};
    apl_status result =
        normalize_fields(serial_number, part_number, condition, notes, &unit, error);
    if (result == APL_OK) {
        result = validate_type(app, team_id, type_id, error);
    }
    if (result != APL_OK) {
        return apl_use_case_with_context(result, "cannot create part unit", error);
    }

    apl_db_transaction transaction = {};
    result = apl_use_case_begin(&transaction, app, "cannot create part unit", error);
    if (result != APL_OK) {
        return result;
    }
    result = apl_part_unit_repository_create(&app->db, &unit, out_unit, error);
    if (result == APL_OK) {
        apl_activity_change changes[6] = {};
        change_id(&changes[0], "type_id", out_unit->type_id);
        apl_audit_change_text(&changes[1], "serial_number", nullptr, true, out_unit->serial_number,
                              out_unit->serial_number[0] == '\0');
        apl_audit_change_text(&changes[2], "part_number", nullptr, true, out_unit->part_number,
                              out_unit->part_number[0] == '\0');
        apl_audit_change_text(&changes[3], "condition", nullptr, true, out_unit->condition,
                              out_unit->condition[0] == '\0');
        apl_audit_change_text(&changes[4], "notes", nullptr, true, out_unit->notes,
                              out_unit->notes[0] == '\0');
        apl_audit_change_text(&changes[5], "archived", nullptr, true, "false", false);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        if (out_unit->serial_number[0] != '\0' || out_unit->part_number[0] != '\0') {
            (void)snprintf(summary, sizeof(summary), "Created part unit '%s%s%s'",
                           out_unit->serial_number, out_unit->serial_number[0] != '\0' ? " " : "",
                           out_unit->part_number);
        } else {
            (void)snprintf(summary, sizeof(summary), "Created part unit #%lld",
                           (long long)out_unit->id);
        }
        result = append_event(app, team_id, "part_unit_created", out_unit->id, summary, changes,
                              APL_COUNT_OF(changes), error);
    }
    if (result != APL_OK) {
        return apl_use_case_rollback(&transaction, result, "cannot create part unit", error);
    }
    return apl_use_case_commit(&transaction, "cannot create part unit", error);
}

apl_status apl_part_unit_service_get(const apl_app *const app, const int64_t team_id,
                                     const int64_t id, apl_part_unit *const out_unit,
                                     apl_error *const error)
{
    if (app == nullptr || out_unit == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part unit get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(
        apl_part_unit_repository_get(&app->db, team_id, id, out_unit, error),
        "cannot load part unit", error);
}

apl_status apl_part_unit_service_update(const apl_app *const app, const int64_t team_id,
                                        const int64_t id, const int64_t type_id,
                                        const char *const serial_number,
                                        const char *const part_number, const char *const condition,
                                        const char *const notes, apl_part_unit *const out_unit,
                                        apl_error *const error)
{
    if (app == nullptr || out_unit == nullptr || team_id <= 0 || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part unit update arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_unit = (apl_part_unit){};

    apl_part_unit unit = {.team_id = team_id, .type_id = type_id};
    apl_status status =
        normalize_fields(serial_number, part_number, condition, notes, &unit, error);
    if (status == APL_OK) {
        status = validate_type(app, team_id, type_id, error);
    }
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot update part unit", error);
    }

    apl_db_transaction transaction = {};
    status = apl_use_case_begin(&transaction, app, "cannot update part unit", error);
    if (status != APL_OK) {
        return status;
    }
    apl_part_unit existing = {};
    status = apl_part_unit_repository_get(&app->db, team_id, id, &existing, error);
    if (status != APL_OK) {
        return apl_use_case_rollback(&transaction, status, "cannot update part unit", error);
    }
    unit.id = existing.id;
    const bool type_changed = existing.type_id != unit.type_id;
    const bool serial_changed = strcmp(existing.serial_number, unit.serial_number) != 0;
    const bool part_changed = strcmp(existing.part_number, unit.part_number) != 0;
    const bool condition_changed = strcmp(existing.condition, unit.condition) != 0;
    const bool notes_changed = strcmp(existing.notes, unit.notes) != 0;
    if (!type_changed && !serial_changed && !part_changed && !condition_changed && !notes_changed) {
        *out_unit = existing;
        return apl_use_case_commit(&transaction, "cannot update part unit", error);
    }

    status = apl_part_unit_repository_update(&app->db, &unit, out_unit, error);
    if (status == APL_OK) {
        apl_activity_change changes[5] = {};
        size_t count = 0;
        if (type_changed) {
            char old_id[32];
            char new_id[32];
            (void)snprintf(old_id, sizeof(old_id), "%lld", (long long)existing.type_id);
            (void)snprintf(new_id, sizeof(new_id), "%lld", (long long)out_unit->type_id);
            apl_audit_change_text(&changes[count++], "type_id", old_id, false, new_id, false);
        }
        if (serial_changed) {
            apl_audit_change_text(&changes[count++], "serial_number", existing.serial_number, false,
                                  out_unit->serial_number, out_unit->serial_number[0] == '\0');
        }
        if (part_changed) {
            apl_audit_change_text(&changes[count++], "part_number", existing.part_number, false,
                                  out_unit->part_number, out_unit->part_number[0] == '\0');
        }
        if (condition_changed) {
            apl_audit_change_text(&changes[count++], "condition", existing.condition, false,
                                  out_unit->condition, out_unit->condition[0] == '\0');
        }
        if (notes_changed) {
            apl_audit_change_text(&changes[count++], "notes", existing.notes, false,
                                  out_unit->notes, out_unit->notes[0] == '\0');
        }
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Updated part unit #%lld",
                       (long long)out_unit->id);
        status =
            append_event(app, team_id, "part_unit_updated", id, summary, changes, count, error);
    }
    if (status != APL_OK) {
        return apl_use_case_rollback(&transaction, status, "cannot update part unit", error);
    }
    return apl_use_case_commit(&transaction, "cannot update part unit", error);
}

apl_status apl_part_unit_service_archive(const apl_app *const app, const int64_t team_id,
                                         const int64_t id, apl_part_unit *const out_unit,
                                         apl_error *const error)
{
    if (app == nullptr || out_unit == nullptr || team_id <= 0 || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part unit archive arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_db_transaction transaction = {};
    apl_status status = apl_use_case_begin(&transaction, app, "cannot archive part unit", error);
    if (status != APL_OK) {
        return status;
    }
    apl_part_unit existing = {};
    status = apl_part_unit_repository_get(&app->db, team_id, id, &existing, error);
    if (status == APL_OK && existing.meta.archived) {
        *out_unit = existing;
        return apl_use_case_commit(&transaction, "cannot archive part unit", error);
    }
    if (status == APL_OK) {
        status = apl_part_unit_repository_archive(&app->db, team_id, id, out_unit, error);
    }
    if (status == APL_OK) {
        apl_activity_change change = {};
        apl_audit_change_bool(&change, "archived", false, true);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Archived part unit #%lld", (long long)id);
        status = append_event(app, team_id, "part_unit_archived", id, summary, &change, 1, error);
    }
    if (status != APL_OK) {
        return apl_use_case_rollback(&transaction, status, "cannot archive part unit", error);
    }
    return apl_use_case_commit(&transaction, "cannot archive part unit", error);
}

apl_status apl_part_unit_service_list(const apl_app *const app,
                                      const apl_part_unit_filter *const filter,
                                      apl_part_unit **const out_units, size_t *const out_count,
                                      apl_error *const error)
{
    if (app == nullptr || filter == nullptr || filter->team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part unit list arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(
        apl_part_unit_repository_list(&app->db, filter, out_units, out_count, error),
        "cannot list part units", error);
}

void apl_part_unit_service_free_list(apl_part_unit *const units)
{
    apl_part_unit_repository_free_list(units);
}