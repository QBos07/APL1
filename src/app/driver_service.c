#include "app/driver_service.h"

#include "db/driver_repository.h"

static apl_status with_context(const apl_status status, const char *const context,
                               apl_error *const error)
{
    if (status != APL_OK) {
        apl_error_prefix(error, status, context);
    }
    return status;
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
    return with_context(apl_driver_repository_create(&app->db, normalized_name, normalized_short,
                                                     normalized_notes, out_driver, error),
                        "cannot create driver", error);
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
    return with_context(apl_driver_repository_update(&app->db, id, normalized_name,
                                                     normalized_short, normalized_notes, out_driver,
                                                     error),
                        "cannot update driver", error);
}

apl_status apl_driver_service_archive(apl_app *const app, const int64_t id,
                                      apl_driver *const out_driver, apl_error *const error)
{
    if (app == nullptr || out_driver == nullptr || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "driver archive arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return with_context(apl_driver_repository_archive(&app->db, id, out_driver, error),
                        "cannot archive driver", error);
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
