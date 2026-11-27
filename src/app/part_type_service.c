#include "app/part_type_service.h"

#include <stdio.h>
#include <string.h>

#include "app/audit.h"
#include "app/use_case.h"
#include "core/util.h"
#include "db/part_type_repository.h"
#include "db/transaction.h"
#include "domain/record.h"

static apl_status normalize_fields(const char *const category, const char *const name,
                                   const char *const description, const char *const notes,
                                   apl_part_type *const type, apl_error *const error)
{
    apl_status result =
        apl_part_type_category_normalize(category, type->category, sizeof(type->category),
                                         type->category_key, sizeof(type->category_key), error);
    if (result == APL_OK) {
        result = apl_part_type_name_normalize(name, type->name, sizeof(type->name), type->name_key,
                                              sizeof(type->name_key), error);
        if (result != APL_OK) {
            apl_error_set_field(error, APL_FIELD_NAME);
        }
    }
    if (result == APL_OK) {
        result = apl_part_type_description_normalize(description, type->description,
                                                     sizeof(type->description), error);
    }
    if (result == APL_OK) {
        result = apl_part_type_notes_normalize(notes, type->notes, sizeof(type->notes), error);
    }
    return result;
}

static apl_status append_event(const apl_app *const app, const int64_t team_id,
                               const char *const action, const int64_t type_id,
                               const char *const summary, const apl_activity_change *const changes,
                               const size_t count, apl_error *const error)
{
    return apl_audit_append(app, team_id, action, apl_record_kind_code(APL_RECORD_PART_TYPE),
                            type_id, summary, changes, count, error);
}

apl_status apl_part_type_service_create(const apl_app *const app, const int64_t team_id,
                                        const char *const category, const char *const name,
                                        const char *const description, const char *const notes,
                                        apl_part_type *const out_type, apl_error *const error)
{
    if (app == nullptr || out_type == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part type create arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_type = (apl_part_type){};

    apl_part_type type = {.team_id = team_id};
    apl_status result = normalize_fields(category, name, description, notes, &type, error);
    if (result != APL_OK) {
        return apl_use_case_with_context(result, "cannot create part type", error);
    }

    apl_db_transaction transaction = {};
    result = apl_use_case_begin(&transaction, app, "cannot create part type", error);
    if (result != APL_OK) {
        return result;
    }
    result = apl_part_type_repository_create(&app->db, &type, out_type, error);
    if (result == APL_OK) {
        apl_activity_change changes[4] = {};
        apl_audit_change_text(&changes[0], "category", nullptr, true, out_type->category, false);
        apl_audit_change_text(&changes[1], "name", nullptr, true, out_type->name, false);
        apl_audit_change_text(&changes[2], "description", nullptr, true, out_type->description,
                              out_type->description[0] == '\0');
        apl_audit_change_text(&changes[3], "notes", nullptr, true, out_type->notes,
                              out_type->notes[0] == '\0');
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Created part type '%s' / '%s'", out_type->name,
                       out_type->category);
        result = append_event(app, team_id, "part_type_created", out_type->id, summary, changes,
                              APL_COUNT_OF(changes), error);
    }
    if (result != APL_OK) {
        return apl_use_case_rollback(&transaction, result, "cannot create part type", error);
    }
    return apl_use_case_commit(&transaction, "cannot create part type", error);
}

apl_status apl_part_type_service_get(const apl_app *const app, const int64_t team_id,
                                     const int64_t id, apl_part_type *const out_type,
                                     apl_error *const error)
{
    if (app == nullptr || out_type == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part type get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(
        apl_part_type_repository_get(&app->db, team_id, id, out_type, error),
        "cannot load part type", error);
}

apl_status apl_part_type_service_update(const apl_app *const app, const int64_t team_id,
                                        const int64_t id, const char *const category,
                                        const char *const name, const char *const description,
                                        const char *const notes, apl_part_type *const out_type,
                                        apl_error *const error)
{
    if (app == nullptr || out_type == nullptr || team_id <= 0 || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part type update arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_type = (apl_part_type){};

    apl_part_type type = {.team_id = team_id};
    apl_status status = normalize_fields(category, name, description, notes, &type, error);
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot update part type", error);
    }

    apl_db_transaction transaction = {};
    status = apl_use_case_begin(&transaction, app, "cannot update part type", error);
    if (status != APL_OK) {
        return status;
    }
    apl_part_type existing = {};
    status = apl_part_type_repository_get(&app->db, team_id, id, &existing, error);
    if (status != APL_OK) {
        return apl_use_case_rollback(&transaction, status, "cannot update part type", error);
    }
    type.id = existing.id;
    const bool category_changed = strcmp(existing.category, type.category) != 0;
    const bool name_changed = strcmp(existing.name, type.name) != 0;
    const bool description_changed = strcmp(existing.description, type.description) != 0;
    const bool notes_changed = strcmp(existing.notes, type.notes) != 0;
    if (!category_changed && !name_changed && !description_changed && !notes_changed) {
        *out_type = existing;
        return apl_use_case_commit(&transaction, "cannot update part type", error);
    }

    status = apl_part_type_repository_update(&app->db, &type, out_type, error);
    if (status == APL_OK) {
        apl_activity_change changes[4] = {};
        size_t count = 0;
        if (category_changed) {
            apl_audit_change_text(&changes[count++], "category", existing.category, false,
                                  out_type->category, false);
        }
        if (name_changed) {
            apl_audit_change_text(&changes[count++], "name", existing.name, false, out_type->name,
                                  false);
        }
        if (description_changed) {
            apl_audit_change_text(&changes[count++], "description", existing.description, false,
                                  out_type->description, out_type->description[0] == '\0');
        }
        if (notes_changed) {
            apl_audit_change_text(&changes[count++], "notes", existing.notes, false,
                                  out_type->notes, out_type->notes[0] == '\0');
        }
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Updated part type '%s' / '%s'", out_type->name,
                       out_type->category);
        status =
            append_event(app, team_id, "part_type_updated", id, summary, changes, count, error);
    }
    if (status != APL_OK) {
        return apl_use_case_rollback(&transaction, status, "cannot update part type", error);
    }
    return apl_use_case_commit(&transaction, "cannot update part type", error);
}

apl_status apl_part_type_service_archive(const apl_app *const app, const int64_t team_id,
                                         const int64_t id, apl_part_type *const out_type,
                                         apl_error *const error)
{
    if (app == nullptr || out_type == nullptr || team_id <= 0 || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part type archive arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_db_transaction transaction = {};
    apl_status status = apl_use_case_begin(&transaction, app, "cannot archive part type", error);
    if (status != APL_OK) {
        return status;
    }
    apl_part_type existing = {};
    status = apl_part_type_repository_get(&app->db, team_id, id, &existing, error);
    if (status == APL_OK && existing.meta.archived) {
        *out_type = existing;
        return apl_use_case_commit(&transaction, "cannot archive part type", error);
    }
    if (status == APL_OK) {
        status = apl_part_type_repository_archive(&app->db, team_id, id, out_type, error);
    }
    if (status == APL_OK) {
        apl_activity_change change = {};
        apl_audit_change_bool(&change, "archived", false, true);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Archived part type '%s' / '%s'", out_type->name,
                       out_type->category);
        status = append_event(app, team_id, "part_type_archived", id, summary, &change, 1, error);
    }
    if (status != APL_OK) {
        return apl_use_case_rollback(&transaction, status, "cannot archive part type", error);
    }
    return apl_use_case_commit(&transaction, "cannot archive part type", error);
}

apl_status apl_part_type_service_list(const apl_app *const app,
                                      const apl_part_type_filter *const filter,
                                      apl_part_type **const out_types, size_t *const out_count,
                                      apl_error *const error)
{
    if (app == nullptr || filter == nullptr || filter->team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part type list arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(
        apl_part_type_repository_list(&app->db, filter, out_types, out_count, error),
        "cannot list part types", error);
}

void apl_part_type_service_free_list(apl_part_type *const types)
{
    apl_part_type_repository_free_list(types);
}