#include "app/season_service.h"

#include "db/season_repository.h"
#include "domain/team.h"

static apl_status with_context(const apl_status status, const char *const context,
                               apl_error *const error)
{
    if (status != APL_OK) {
        apl_error_prefix(error, status, context);
    }
    return status;
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
    return with_context(apl_season_repository_create(&app->db, team_id, normalized_label,
                                                     normalized_start, normalized_end,
                                                     normalized_notes, out_season, error),
                        "cannot create season", error);
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
    return with_context(apl_season_repository_update(&app->db, team_id, id, normalized_label,
                                                     normalized_start, normalized_end,
                                                     normalized_notes, out_season, error),
                        "cannot update season", error);
}

apl_status apl_season_service_archive(apl_app *const app, const int64_t team_id, const int64_t id,
                                      apl_season *const out_season, apl_error *const error)
{
    if (app == nullptr || out_season == nullptr || team_id <= 0 || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "season archive arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return with_context(apl_season_repository_archive(&app->db, team_id, id, out_season, error),
                        "cannot archive season", error);
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
