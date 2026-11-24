#include "app/car_service.h"

#include "db/car_repository.h"

static apl_status with_context(const apl_status status, const char *const context,
                               apl_error *const error)
{
    if (status != APL_OK) {
        apl_error_prefix(error, status, context);
    }
    return status;
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

apl_status apl_car_service_create(apl_app *const app, const int64_t team_id,
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
    return with_context(apl_car_repository_create(&app->db, team_id, normalized_label,
                                                  normalized_chassis, normalized_notes, out_car,
                                                  error),
                        "cannot create car", error);
}

apl_status apl_car_service_get(apl_app *const app, const int64_t team_id, const int64_t id,
                               apl_car *const out_car, apl_error *const error)
{
    if (app == nullptr || out_car == nullptr || team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "car get arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return with_context(apl_car_repository_get(&app->db, team_id, id, out_car, error),
                        "cannot load car", error);
}

apl_status apl_car_service_update(apl_app *const app, const int64_t team_id, const int64_t id,
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
    return with_context(apl_car_repository_update(&app->db, team_id, id, normalized_label,
                                                  normalized_chassis, normalized_notes, out_car,
                                                  error),
                        "cannot update car", error);
}

apl_status apl_car_service_archive(apl_app *const app, const int64_t team_id, const int64_t id,
                                   apl_car *const out_car, apl_error *const error)
{
    if (app == nullptr || out_car == nullptr || team_id <= 0 || id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "car archive arguments are invalid");
        return APL_ERR_INVALID_ARGUMENT;
    }
    return with_context(apl_car_repository_archive(&app->db, team_id, id, out_car, error),
                        "cannot archive car", error);
}

apl_status apl_car_service_list(apl_app *const app, const int64_t team_id,
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
