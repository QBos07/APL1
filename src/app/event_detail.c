#include "app/event_detail.h"

#include <stdlib.h>

#include "db/driver_repository.h"

apl_status apl_event_detail_load(const apl_app *const app, const int64_t team_id,
                                 const int64_t event_id, apl_event_detail *const out,
                                 apl_error *const error)
{
    if (!app || !out || team_id <= 0 || event_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid event detail arguments");
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_event_detail detail = {};
    apl_status status = apl_event_repository_get(&app->db, team_id, event_id, &detail.event, error);
    if (status == APL_OK)
        status = apl_session_repository_list(&app->db, team_id, event_id, &detail.sessions,
                                             &detail.session_count, error);
    if (status == APL_OK)
        *out = detail;
    return status;
}

void apl_event_detail_free(apl_event_detail *const detail)
{
    if (!detail)
        return;
    apl_session_repository_free_list(detail->sessions);
    *detail = (apl_event_detail){};
}

apl_status apl_session_detail_load(const apl_app *const app, const int64_t team_id,
                                   const int64_t event_id, const int64_t session_id,
                                   apl_session_detail *const out, apl_error *const error)
{
    if (!app || !out || team_id <= 0 || event_id <= 0 || session_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "invalid session detail arguments");
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_session_detail detail = {};
    apl_status status = apl_event_repository_get(&app->db, team_id, event_id, &detail.event, error);
    if (status == APL_OK)
        status = apl_session_repository_get(&app->db, team_id, event_id, session_id,
                                            &detail.session, error);
    apl_driver *drivers = nullptr;
    size_t driver_count = 0;
    if (status == APL_OK)
        status = apl_driver_repository_list_for_team(&app->db, team_id, true, &drivers,
                                                     &driver_count, error);
    apl_outcome *outcomes = nullptr;
    size_t outcome_count = 0;
    if (status == APL_OK)
        status = apl_outcome_repository_list(&app->db, team_id, event_id, session_id, &outcomes,
                                             &outcome_count, error);
    if (status == APL_OK && driver_count > 0) {
        detail.rows = calloc(driver_count, sizeof(*detail.rows));
        if (!detail.rows) {
            apl_error_set(error, APL_ERR_OUT_OF_MEMORY, "cannot allocate session detail");
            status = APL_ERR_OUT_OF_MEMORY;
        }
    }
    if (status == APL_OK) {
        detail.row_count = driver_count;
        for (size_t i = 0; i < driver_count; ++i) {
            detail.rows[i].driver = drivers[i];
            for (size_t j = 0; j < outcome_count; ++j) {
                if (outcomes[j].driver_id == drivers[i].id) {
                    detail.rows[i].has_outcome = true;
                    detail.rows[i].outcome = outcomes[j];
                    break;
                }
            }
        }
        *out = detail;
    }
    apl_driver_repository_free_list(drivers);
    apl_outcome_repository_free_list(outcomes);
    return status;
}

void apl_session_detail_free(apl_session_detail *const detail)
{
    if (!detail)
        return;
    free(detail->rows);
    *detail = (apl_session_detail){};
}
