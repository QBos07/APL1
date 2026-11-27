#ifndef APL_DB_SESSION_REPOSITORY_H
#define APL_DB_SESSION_REPOSITORY_H

#include "db/connection.h"
#include "domain/reference.h"

typedef struct apl_event_session {
    int64_t id;
    int64_t team_id;
    int64_t event_id;
    char label[APL_REFERENCE_LABEL_MAX];
    char created_at[APL_REFERENCE_TIMESTAMP_MAX];
    char updated_at[APL_REFERENCE_TIMESTAMP_MAX];
} apl_event_session;

apl_status apl_session_repository_create(const apl_db_connection *connection, int64_t team_id,
                                         int64_t event_id, const char *label,
                                         apl_event_session *out, apl_error *error);
apl_status apl_session_repository_get(const apl_db_connection *connection, int64_t team_id,
                                      int64_t event_id, int64_t id, apl_event_session *out,
                                      apl_error *error);
/* Edits the label only: a session's owning event is stable. */
apl_status apl_session_repository_update(const apl_db_connection *connection, int64_t team_id,
                                         int64_t event_id, int64_t id, const char *label,
                                         apl_event_session *out, apl_error *error);
apl_status apl_session_repository_list(const apl_db_connection *connection, int64_t team_id,
                                       int64_t event_id, apl_event_session **out, size_t *count,
                                       apl_error *error);
void apl_session_repository_free_list(apl_event_session *sessions);

#endif
