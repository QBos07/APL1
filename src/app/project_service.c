#include "app/project_service.h"

#include <stdio.h>
#include <string.h>

#include "app/audit.h"
#include "app/use_case.h"
#include "core/util.h"
#include "db/car_repository.h"
#include "db/project_repository.h"
#include "db/project_update_repository.h"
#include "db/season_repository.h"
#include "db/transaction.h"
#include "domain/record.h"

static apl_status normalize_fields(const char *const title, const char *const category,
                                   const char *const description, const char *const status,
                                   const char *const target_date, const char *const actual_date,
                                   const char *const notes, apl_project *const project,
                                   apl_error *const error)
{
    apl_status result =
        apl_project_title_normalize(title, project->title, sizeof(project->title), error);
    if (result == APL_OK) {
        result = apl_project_label_normalize(category, "project category", project->category,
                                             sizeof(project->category), project->category_key,
                                             sizeof(project->category_key), error);
        if (result != APL_OK) {
            apl_error_set_field(error, APL_FIELD_CATEGORY);
        }
    }
    if (result == APL_OK) {
        result = apl_project_description_normalize(description, project->description,
                                                   sizeof(project->description), error);
    }
    if (result == APL_OK) {
        result = apl_project_label_normalize(status, "project status", project->status,
                                             sizeof(project->status), project->status_key,
                                             sizeof(project->status_key), error);
        if (result != APL_OK) {
            apl_error_set_field(error, APL_FIELD_PROJECT_STATUS);
        }
    }
    if (result == APL_OK) {
        result = apl_reference_date_normalize(target_date, "target date", project->target_date,
                                              sizeof(project->target_date), error);
        if (result != APL_OK) {
            apl_error_set_field(error, APL_FIELD_TARGET_DATE);
        }
    }
    if (result == APL_OK) {
        result = apl_reference_date_normalize(actual_date, "actual date", project->actual_date,
                                              sizeof(project->actual_date), error);
        if (result != APL_OK) {
            apl_error_set_field(error, APL_FIELD_ACTUAL_DATE);
        }
    }
    if (result == APL_OK) {
        result = apl_project_notes_normalize(notes, project->notes, sizeof(project->notes), error);
    }
    return result;
}

static apl_status validate_links(const apl_app *const app, const int64_t team_id,
                                 const int64_t season_id, const int64_t car_id,
                                 apl_error *const error)
{
    if (season_id < 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "project season link must be a positive ID or empty");
        apl_error_set_field(error, APL_FIELD_SEASON);
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (car_id < 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "project car link must be a positive ID or empty");
        apl_error_set_field(error, APL_FIELD_CAR);
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (season_id > 0) {
        apl_season season = {};
        const apl_status status =
            apl_season_repository_get(&app->db, team_id, season_id, &season, error);
        if (status != APL_OK) {
            apl_error_set_field(error, APL_FIELD_SEASON);
            return status;
        }
    }
    if (car_id > 0) {
        apl_car car = {};
        const apl_status status = apl_car_repository_get(&app->db, team_id, car_id, &car, error);
        if (status != APL_OK) {
            apl_error_set_field(error, APL_FIELD_CAR);
            return status;
        }
    }
    return APL_OK;
}

static void change_optional_id(apl_activity_change *const change, const char *const field,
                               const int64_t old_id, const int64_t new_id)
{
    char old_text[32];
    char new_text[32];
    const bool old_missing = old_id <= 0;
    const bool new_missing = new_id <= 0;
    if (!old_missing) {
        (void)snprintf(old_text, sizeof(old_text), "%lld", (long long)old_id);
    }
    if (!new_missing) {
        (void)snprintf(new_text, sizeof(new_text), "%lld", (long long)new_id);
    }
    apl_audit_change_text(change, field, old_missing ? nullptr : old_text, old_missing,
                          new_missing ? nullptr : new_text, new_missing);
}

static void change_id(apl_activity_change *const change, const char *const field,
                      const int64_t value)
{
    change_optional_id(change, field, 0, value);
}

static apl_status append_event(const apl_app *const app, const int64_t team_id,
                               const char *const action, const int64_t project_id,
                               const char *const summary, const apl_activity_change *const changes,
                               const size_t count, apl_error *const error)
{
    return apl_audit_append(app, team_id, action, apl_record_kind_code(APL_RECORD_PROJECT),
                            project_id, summary, changes, count, error);
}

apl_status apl_project_service_create(const apl_app *const app, const int64_t team_id,
                                      const char *const title, const char *const category,
                                      const char *const description, const char *const status_text,
                                      const char *const target_date, const char *const actual_date,
                                      const int64_t season_id, const int64_t car_id,
                                      const char *const notes, apl_project *const out_project,
                                      apl_error *const error)
{
    if (app == nullptr || out_project == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "project create arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_project = (apl_project){};

    apl_project project = {.team_id = team_id, .season_id = season_id, .car_id = car_id};
    apl_status result = normalize_fields(title, category, description, status_text, target_date,
                                         actual_date, notes, &project, error);
    if (result == APL_OK) {
        result = validate_links(app, team_id, season_id, car_id, error);
    }
    if (result != APL_OK) {
        return apl_use_case_with_context(result, "cannot create project", error);
    }

    apl_db_transaction transaction = {};
    result = apl_use_case_begin(&transaction, app, "cannot create project", error);
    if (result != APL_OK) {
        return result;
    }
    result = apl_project_repository_create(&app->db, &project, out_project, error);
    if (result == APL_OK) {
        apl_activity_change changes[9] = {};
        apl_audit_change_text(&changes[0], "title", nullptr, true, out_project->title, false);
        apl_audit_change_text(&changes[1], "category", nullptr, true, out_project->category,
                              out_project->category[0] == '\0');
        apl_audit_change_text(&changes[2], "description", nullptr, true, out_project->description,
                              out_project->description[0] == '\0');
        apl_audit_change_text(&changes[3], "status", nullptr, true, out_project->status,
                              out_project->status[0] == '\0');
        apl_audit_change_text(&changes[4], "target_date", nullptr, true, out_project->target_date,
                              out_project->target_date[0] == '\0');
        apl_audit_change_text(&changes[5], "actual_date", nullptr, true, out_project->actual_date,
                              out_project->actual_date[0] == '\0');
        change_id(&changes[6], "season_id", out_project->season_id);
        change_id(&changes[7], "car_id", out_project->car_id);
        apl_audit_change_text(&changes[8], "notes", nullptr, true, out_project->notes,
                              out_project->notes[0] == '\0');
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Created project '%s'", out_project->title);
        result = append_event(app, team_id, "project_created", out_project->id, summary, changes,
                              APL_COUNT_OF(changes), error);
    }
    if (result != APL_OK) {
        return apl_use_case_rollback(&transaction, result, "cannot create project", error);
    }
    return apl_use_case_commit(&transaction, "cannot create project", error);
}

apl_status apl_project_service_get(const apl_app *const app, const int64_t team_id,
                                   const int64_t id, apl_project *const out_project,
                                   apl_error *const error)
{
    if (app == nullptr || out_project == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "project get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(
        apl_project_repository_get(&app->db, team_id, id, out_project, error),
        "cannot load project", error);
}

apl_status apl_project_service_update(const apl_app *const app, const int64_t team_id,
                                      const int64_t id, const char *const title,
                                      const char *const category, const char *const description,
                                      const char *const status_text, const char *const target_date,
                                      const char *const actual_date, const int64_t season_id,
                                      const int64_t car_id, const char *const notes,
                                      apl_project *const out_project, apl_error *const error)
{
    if (app == nullptr || out_project == nullptr || team_id <= 0 || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "project update arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_project = (apl_project){};

    apl_project project = {.team_id = team_id, .season_id = season_id, .car_id = car_id};
    apl_status status = normalize_fields(title, category, description, status_text, target_date,
                                         actual_date, notes, &project, error);
    if (status == APL_OK) {
        status = validate_links(app, team_id, season_id, car_id, error);
    }
    if (status != APL_OK) {
        return apl_use_case_with_context(status, "cannot update project", error);
    }

    apl_db_transaction transaction = {};
    status = apl_use_case_begin(&transaction, app, "cannot update project", error);
    if (status != APL_OK) {
        return status;
    }
    apl_project existing = {};
    status = apl_project_repository_get(&app->db, team_id, id, &existing, error);
    if (status != APL_OK) {
        return apl_use_case_rollback(&transaction, status, "cannot update project", error);
    }
    project.id = existing.id;
    const bool title_changed = strcmp(existing.title, project.title) != 0;
    const bool category_changed = strcmp(existing.category, project.category) != 0;
    const bool description_changed = strcmp(existing.description, project.description) != 0;
    const bool status_changed = strcmp(existing.status, project.status) != 0;
    const bool target_changed = strcmp(existing.target_date, project.target_date) != 0;
    const bool actual_changed = strcmp(existing.actual_date, project.actual_date) != 0;
    const bool season_changed = existing.season_id != project.season_id;
    const bool car_changed = existing.car_id != project.car_id;
    const bool notes_changed = strcmp(existing.notes, project.notes) != 0;
    if (!title_changed && !category_changed && !description_changed && !status_changed &&
        !target_changed && !actual_changed && !season_changed && !car_changed && !notes_changed) {
        *out_project = existing;
        return apl_use_case_commit(&transaction, "cannot update project", error);
    }

    status = apl_project_repository_update(&app->db, &project, out_project, error);
    if (status == APL_OK) {
        apl_activity_change changes[9] = {};
        size_t count = 0;
        if (title_changed) {
            apl_audit_change_text(&changes[count++], "title", existing.title, false,
                                  out_project->title, false);
        }
        if (category_changed) {
            apl_audit_change_text(&changes[count++], "category", existing.category, false,
                                  out_project->category, out_project->category[0] == '\0');
        }
        if (description_changed) {
            apl_audit_change_text(&changes[count++], "description", existing.description, false,
                                  out_project->description, out_project->description[0] == '\0');
        }
        if (status_changed) {
            apl_audit_change_text(&changes[count++], "status", existing.status, false,
                                  out_project->status, out_project->status[0] == '\0');
        }
        if (target_changed) {
            apl_audit_change_text(&changes[count++], "target_date", existing.target_date, false,
                                  out_project->target_date, out_project->target_date[0] == '\0');
        }
        if (actual_changed) {
            apl_audit_change_text(&changes[count++], "actual_date", existing.actual_date, false,
                                  out_project->actual_date, out_project->actual_date[0] == '\0');
        }
        if (season_changed) {
            change_optional_id(&changes[count++], "season_id", existing.season_id,
                               out_project->season_id);
        }
        if (car_changed) {
            change_optional_id(&changes[count++], "car_id", existing.car_id, out_project->car_id);
        }
        if (notes_changed) {
            apl_audit_change_text(&changes[count++], "notes", existing.notes, false,
                                  out_project->notes, out_project->notes[0] == '\0');
        }
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Updated project '%s'", out_project->title);
        status = append_event(app, team_id, "project_updated", id, summary, changes, count, error);
    }
    if (status != APL_OK) {
        return apl_use_case_rollback(&transaction, status, "cannot update project", error);
    }
    return apl_use_case_commit(&transaction, "cannot update project", error);
}

apl_status apl_project_service_archive(const apl_app *const app, const int64_t team_id,
                                       const int64_t id, apl_project *const out_project,
                                       apl_error *const error)
{
    if (app == nullptr || out_project == nullptr || team_id <= 0 || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "project archive arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_db_transaction transaction = {};
    apl_status status = apl_use_case_begin(&transaction, app, "cannot archive project", error);
    if (status != APL_OK) {
        return status;
    }
    apl_project existing = {};
    status = apl_project_repository_get(&app->db, team_id, id, &existing, error);
    if (status == APL_OK && existing.meta.archived) {
        *out_project = existing;
        return apl_use_case_commit(&transaction, "cannot archive project", error);
    }
    if (status == APL_OK) {
        status = apl_project_repository_archive(&app->db, team_id, id, out_project, error);
    }
    if (status == APL_OK) {
        apl_activity_change change = {};
        apl_audit_change_bool(&change, "archived", false, true);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Archived project '%s'", out_project->title);
        status = append_event(app, team_id, "project_archived", id, summary, &change, 1, error);
    }
    if (status != APL_OK) {
        return apl_use_case_rollback(&transaction, status, "cannot archive project", error);
    }
    return apl_use_case_commit(&transaction, "cannot archive project", error);
}

apl_status apl_project_service_list(const apl_app *const app,
                                    const apl_project_filter *const filter,
                                    apl_project **const out_projects, size_t *const out_count,
                                    apl_error *const error)
{
    if (app == nullptr || filter == nullptr || filter->team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "project list arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(
        apl_project_repository_list(&app->db, filter, out_projects, out_count, error),
        "cannot list projects", error);
}

static apl_status normalize_update_fields(const char *const update_date, const char *const summary,
                                          const char *const status_change, const char *const notes,
                                          apl_project_update *const update, apl_error *const error)
{
    apl_status result = apl_reference_date_normalize(
        update_date, "update date", update->update_date, sizeof(update->update_date), error);
    if (result == APL_OK && update->update_date[0] == '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "update date is required");
        result = APL_ERR_INVALID_ARGUMENT;
    }
    if (result != APL_OK) {
        apl_error_set_field(error, APL_FIELD_UPDATE_DATE);
        return result;
    }
    result = apl_reference_label_normalize(summary, "update summary", update->summary,
                                           sizeof(update->summary), error);
    if (result != APL_OK) {
        apl_error_set_field(error, APL_FIELD_SUMMARY);
        return result;
    }
    result = apl_project_label_normalize(status_change, "status change", update->status_change,
                                         sizeof(update->status_change), update->status_change_key,
                                         sizeof(update->status_change_key), error);
    if (result != APL_OK) {
        apl_error_set_field(error, APL_FIELD_PROJECT_STATUS);
        return result;
    }
    result = apl_project_notes_normalize(notes, update->notes, sizeof(update->notes), error);
    return result;
}

apl_status apl_project_service_log_update(const apl_app *const app, const int64_t team_id,
                                          const int64_t project_id, const char *const update_date,
                                          const char *const summary,
                                          const char *const status_change, const char *const notes,
                                          apl_project_update *const out_update,
                                          apl_project *const out_project, apl_error *const error)
{
    if (app == nullptr || out_update == nullptr || out_project == nullptr || team_id <= 0 ||
        project_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "project update arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_update = (apl_project_update){};
    *out_project = (apl_project){};

    apl_project_update update = {.project_id = project_id};
    apl_status result =
        normalize_update_fields(update_date, summary, status_change, notes, &update, error);
    if (result != APL_OK) {
        return apl_use_case_with_context(result, "cannot log project update", error);
    }

    apl_db_transaction transaction = {};
    result = apl_use_case_begin(&transaction, app, "cannot log project update", error);
    if (result != APL_OK) {
        return result;
    }
    apl_project project = {};
    result = apl_project_repository_get(&app->db, team_id, project_id, &project, error);
    if (result == APL_OK && project.meta.archived) {
        apl_error_set(error, APL_ERR_CONFLICT,
                      "archived projects cannot receive new updates; choose an active project");
        result = APL_ERR_CONFLICT;
    }
    if (result == APL_OK) {
        result =
            apl_project_update_repository_append(&app->db, team_id, &update, out_update, error);
    }

    apl_project updated = project;
    const bool changes_status = update.status_change_key[0] != '\0' &&
                                strcmp(project.status_key, update.status_change_key) != 0;
    if (result == APL_OK && changes_status) {
        (void)snprintf(updated.status, sizeof(updated.status), "%s", update.status_change);
        (void)snprintf(updated.status_key, sizeof(updated.status_key), "%s",
                       update.status_change_key);
        apl_project stored = {};
        result = apl_project_repository_update(&app->db, &updated, &stored, error);
        if (result == APL_OK) {
            updated = stored;
        }
    }
    if (result == APL_OK) {
        apl_activity_change changes[4] = {};
        apl_audit_change_text(&changes[0], "date", nullptr, true, out_update->update_date, false);
        apl_audit_change_text(&changes[1], "summary", nullptr, true, out_update->summary, false);
        apl_audit_change_text(&changes[2], "status_change", nullptr, true,
                              out_update->status_change, out_update->status_change[0] == '\0');
        apl_audit_change_text(&changes[3], "notes", nullptr, true, out_update->notes,
                              out_update->notes[0] == '\0');
        char summary_text[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary_text, sizeof(summary_text), "Logged update for project '%s' on %s",
                       project.title, out_update->update_date);
        result = apl_audit_append(app, team_id, "project_update_logged",
                                  apl_record_kind_code(APL_RECORD_PROJECT_UPDATE), out_update->id,
                                  summary_text, changes, APL_COUNT_OF(changes), error);
    }
    if (result == APL_OK && changes_status) {
        apl_activity_change change = {};
        apl_audit_change_text(&change, "status", project.status, false, updated.status, false);
        char summary_text[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary_text, sizeof(summary_text), "Project '%s' status changed to '%s'",
                       updated.title, updated.status);
        result = apl_audit_append(app, team_id, "project_status_changed",
                                  apl_record_kind_code(APL_RECORD_PROJECT), project.id,
                                  summary_text, &change, 1, error);
    }
    if (result != APL_OK) {
        return apl_use_case_rollback(&transaction, result, "cannot log project update", error);
    }
    result = apl_use_case_commit(&transaction, "cannot log project update", error);
    if (result == APL_OK) {
        *out_project = changes_status ? updated : project;
    }
    return result;
}

apl_status apl_project_service_get_update(const apl_app *const app, const int64_t team_id,
                                          const int64_t id, apl_project_update *const out_update,
                                          apl_error *const error)
{
    if (app == nullptr || out_update == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "project update get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(
        apl_project_update_repository_get(&app->db, team_id, id, out_update, error),
        "cannot load project update", error);
}

apl_status apl_project_service_list_updates(const apl_app *const app, const int64_t team_id,
                                            const int64_t project_id,
                                            apl_project_update **const out_updates,
                                            size_t *const out_count, apl_error *const error)
{
    if (app == nullptr || out_updates == nullptr || out_count == nullptr || team_id <= 0 ||
        project_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "project update list arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return apl_use_case_with_context(apl_project_update_repository_list(&app->db, team_id,
                                                                        project_id, out_updates,
                                                                        out_count, error),
                                     "cannot list project updates", error);
}

void apl_project_service_free_list(apl_project *const projects)
{
    apl_project_repository_free_list(projects);
}

void apl_project_service_free_updates(apl_project_update *const updates)
{
    apl_project_update_repository_free_list(updates);
}
