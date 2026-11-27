#ifndef APL_APP_EVENT_DETAIL_H
#define APL_APP_EVENT_DETAIL_H

#include "app/app.h"
#include "db/event_repository.h"
#include "db/outcome_repository.h"
#include "db/session_repository.h"

typedef struct apl_event_detail {
    apl_event event;
    apl_event_session *sessions;
    size_t session_count;
} apl_event_detail;

typedef struct apl_session_outcome_row {
    apl_driver driver;
    bool has_outcome;
    apl_outcome outcome;
} apl_session_outcome_row;

typedef struct apl_session_detail {
    apl_event event;
    apl_event_session session;
    apl_session_outcome_row *rows;
    size_t row_count;
} apl_session_detail;

/* Lists all drivers with a team membership, including historical and archived
 * drivers. This is a directory, not a claim that any driver participated. */
apl_status apl_event_detail_load(const apl_app *app, int64_t team_id, int64_t event_id,
                                 apl_event_detail *out, apl_error *error);
void apl_event_detail_free(apl_event_detail *detail);
apl_status apl_session_detail_load(const apl_app *app, int64_t team_id, int64_t event_id,
                                   int64_t session_id, apl_session_detail *out, apl_error *error);
void apl_session_detail_free(apl_session_detail *detail);

#endif
