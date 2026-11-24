#include "app/season_service.h"

#include <stdio.h>
#include <string.h>

#include "app/audit.h"
#include "db/season_repository.h"
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

static apl_status normalize_fields(const char *const label, const char *const start_date,
                                   const char *const end_date, const char *const notes,
                                   char *const out_label, char *const out_start,
                                   char *const out_end, char *const out_notes,
                                   apl_error *const error)
{
    apl_status status = apl_reference_label_normalize(label, "season label", out_label,
                                                      APL_REFERENCE_LABEL_MAX, error);
    if (status == APL_OK) {
        status = apl_reference_date_normalize(start_date, "season start date", out_start,
                                              APL_REFERENCE_DATE_MAX, error);
    }
    if (status == APL_OK) {
        status = apl_reference_date_normalize(end_date, "season end date", out_end,
                                              APL_REFERENCE_DATE_MAX, error);
    }
    if (status == APL_OK) {
        status = apl_reference_date_range_validate(out_start, out_end, "season", error);
    }
    if (status == APL_OK) {
        status = apl_reference_optional_normalize(notes, "season notes", out_notes,
                                                  APL_REFERENCE_NOTES_MAX, error);
    }
    return status;
}

static apl_status append_event(apl_app *const app, const int64_t team_id, const char *const action,
                               const int64_t season_id, const char *const summary,
                               const apl_activity_change *const changes, const size_t count,
                               apl_error *const error)
{
    return apl_audit_append(app, team_id, action, "season", season_id, summary, changes, count,
                            error);
}

apl_status apl_season_service_create(apl_app *const app, const int64_t team_id,
                                     const char *const label, const char *const start_date,
                                     const char *const end_date, const char *const notes,
                                     apl_season *const out_season, apl_error *const error)
{
    if (app == nullptr || out_season == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "season create arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    char normalized_label[APL_REFERENCE_LABEL_MAX];
    char normalized_start[APL_REFERENCE_DATE_MAX];
    char normalized_end[APL_REFERENCE_DATE_MAX];
    char normalized_notes[APL_REFERENCE_NOTES_MAX];
    apl_status status = normalize_fields(label, start_date, end_date, notes, normalized_label,
                                         normalized_start, normalized_end, normalized_notes, error);
    if (status != APL_OK) {
        return with_context(status, "cannot create season", error);
    }

    apl_db_transaction transaction = {};
    status = apl_db_transaction_begin(&transaction, &app->db, error);
    if (status != APL_OK) {
        return with_context(status, "cannot create season", error);
    }
    status = apl_season_repository_create(&app->db, team_id, normalized_label, normalized_start,
                                          normalized_end, normalized_notes, out_season, error);
    if (status == APL_OK) {
        apl_activity_change changes[4] = {};
        apl_audit_change_text(&changes[0], "label", nullptr, true, out_season->label, false);
        apl_audit_change_text(&changes[1], "start_date", nullptr, true, out_season->start_date,
                              out_season->start_date[0] == '\0');
        apl_audit_change_text(&changes[2], "end_date", nullptr, true, out_season->end_date,
                              out_season->end_date[0] == '\0');
        apl_audit_change_text(&changes[3], "notes", nullptr, true, out_season->notes, false);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Created season '%s'", out_season->label);
        status = append_event(app, team_id, "season_created", out_season->id, summary, changes, 4,
                              error);
    }
    if (status != APL_OK) {
        return rollback(&transaction, status, "cannot create season", error);
    }
    return commit(&transaction, "cannot create season", error);
}

apl_status apl_season_service_get(apl_app *const app, const int64_t team_id, const int64_t id,
                                  apl_season *const out_season, apl_error *const error)
{
    if (app == nullptr || out_season == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "season get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return with_context(apl_season_repository_get(&app->db, team_id, id, out_season, error),
                        "cannot load season", error);
}

apl_status apl_season_service_update(apl_app *const app, const int64_t team_id, const int64_t id,
                                     const char *const label, const char *const start_date,
                                     const char *const end_date, const char *const notes,
                                     apl_season *const out_season, apl_error *const error)
{
    if (app == nullptr || out_season == nullptr || team_id <= 0 || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "season update arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    char normalized_label[APL_REFERENCE_LABEL_MAX];
    char normalized_start[APL_REFERENCE_DATE_MAX];
    char normalized_end[APL_REFERENCE_DATE_MAX];
    char normalized_notes[APL_REFERENCE_NOTES_MAX];
    apl_status status = normalize_fields(label, start_date, end_date, notes, normalized_label,
                                         normalized_start, normalized_end, normalized_notes, error);
    if (status != APL_OK) {
        return with_context(status, "cannot update season", error);
    }

    apl_db_transaction transaction = {};
    status = apl_db_transaction_begin(&transaction, &app->db, error);
    if (status != APL_OK) {
        return with_context(status, "cannot update season", error);
    }
    apl_season existing = {};
    status = apl_season_repository_get(&app->db, team_id, id, &existing, error);
    if (status != APL_OK) {
        return rollback(&transaction, status, "cannot update season", error);
    }
    const bool label_changed = strcmp(existing.label, normalized_label) != 0;
    const bool start_changed = strcmp(existing.start_date, normalized_start) != 0;
    const bool end_changed = strcmp(existing.end_date, normalized_end) != 0;
    const bool notes_changed = strcmp(existing.notes, normalized_notes) != 0;
    if (!label_changed && !start_changed && !end_changed && !notes_changed) {
        *out_season = existing;
        return commit(&transaction, "cannot update season", error);
    }
    status = apl_season_repository_update(&app->db, team_id, id, normalized_label, normalized_start,
                                          normalized_end, normalized_notes, out_season, error);
    if (status == APL_OK) {
        apl_activity_change changes[4] = {};
        size_t count = 0;
        if (label_changed) {
            apl_audit_change_text(&changes[count++], "label", existing.label, false,
                                  out_season->label, false);
        }
        if (start_changed) {
            apl_audit_change_text(&changes[count++], "start_date", existing.start_date, false,
                                  out_season->start_date, out_season->start_date[0] == '\0');
        }
        if (end_changed) {
            apl_audit_change_text(&changes[count++], "end_date", existing.end_date, false,
                                  out_season->end_date, out_season->end_date[0] == '\0');
        }
        if (notes_changed) {
            apl_audit_change_text(&changes[count++], "notes", existing.notes, false,
                                  out_season->notes, false);
        }
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Updated season '%s'", out_season->label);
        status = append_event(app, team_id, "season_updated", id, summary, changes, count, error);
    }
    if (status != APL_OK) {
        return rollback(&transaction, status, "cannot update season", error);
    }
    return commit(&transaction, "cannot update season", error);
}

apl_status apl_season_service_archive(apl_app *const app, const int64_t team_id, const int64_t id,
                                      apl_season *const out_season, apl_error *const error)
{
    if (app == nullptr || out_season == nullptr || team_id <= 0 || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "season archive arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_db_transaction transaction = {};
    apl_status status = apl_db_transaction_begin(&transaction, &app->db, error);
    if (status != APL_OK) {
        return with_context(status, "cannot archive season", error);
    }
    apl_season existing = {};
    status = apl_season_repository_get(&app->db, team_id, id, &existing, error);
    if (status == APL_OK && existing.archived) {
        *out_season = existing;
        return commit(&transaction, "cannot archive season", error);
    }
    if (status == APL_OK) {
        status = apl_season_repository_archive(&app->db, team_id, id, out_season, error);
    }
    if (status == APL_OK) {
        apl_activity_change change = {};
        apl_audit_change_bool(&change, "archived", false, true);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        (void)snprintf(summary, sizeof(summary), "Archived season '%s'", out_season->label);
        status = append_event(app, team_id, "season_archived", id, summary, &change, 1, error);
    }
    if (status != APL_OK) {
        return rollback(&transaction, status, "cannot archive season", error);
    }
    return commit(&transaction, "cannot archive season", error);
}

apl_status apl_season_service_list(apl_app *const app, const int64_t team_id,
                                   const bool include_archived, apl_season **const out_seasons,
                                   size_t *const out_count, apl_error *const error)
{
    if (app == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "season list arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return with_context(apl_season_repository_list(&app->db, team_id, include_archived, out_seasons,
                                                   out_count, error),
                        "cannot list seasons", error);
}

void apl_season_service_free_list(apl_season *const seasons)
{
    apl_season_repository_free_list(seasons);
}
