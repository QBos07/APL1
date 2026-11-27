#ifndef APL_APP_EVENT_SERVICE_H
#define APL_APP_EVENT_SERVICE_H

#include "app/app.h"
#include "db/event_repository.h"
#include "db/session_repository.h"

/* Events and sessions have stable identities. No destructive delete or archive
 * operation is exposed; linked history remains attached when details change. */
apl_status apl_event_service_create(const apl_app *app, int64_t team_id, int64_t season_id,
                                    const char *name, const char *date, const char *location,
                                    const char *notes, apl_event *out, apl_error *error);
apl_status apl_event_service_update(const apl_app *app, int64_t team_id, int64_t id,
                                    int64_t season_id, const char *name, const char *date,
                                    const char *location, const char *notes, apl_event *out,
                                    apl_error *error);
apl_status apl_session_service_create(const apl_app *app, int64_t team_id, int64_t event_id,
                                      const char *label, apl_event_session *out, apl_error *error);
apl_status apl_session_service_update(const apl_app *app, int64_t team_id, int64_t event_id,
                                      int64_t id, const char *label, apl_event_session *out,
                                      apl_error *error);

#endif
