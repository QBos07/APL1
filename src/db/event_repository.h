#ifndef APL_DB_EVENT_REPOSITORY_H
#define APL_DB_EVENT_REPOSITORY_H

#include "db/connection.h"
#include "domain/reference.h"

typedef struct apl_event {
    int64_t id;
    int64_t team_id;
    int64_t season_id; /* zero means no season */
    char name[APL_REFERENCE_LABEL_MAX];
    char date[APL_REFERENCE_DATE_MAX];
    char location[APL_REFERENCE_LABEL_MAX];
    char notes[APL_REFERENCE_NOTES_MAX];
    char created_at[APL_REFERENCE_TIMESTAMP_MAX];
    char updated_at[APL_REFERENCE_TIMESTAMP_MAX];
} apl_event;

/* season_id is zero when absent. All operations are team-scoped. */
apl_status apl_event_repository_create(const apl_db_connection *connection, int64_t team_id,
                                       int64_t season_id, const char *name, const char *date,
                                       const char *location, const char *notes, apl_event *out,
                                       apl_error *error);
apl_status apl_event_repository_update(const apl_db_connection *connection, int64_t team_id,
                                       int64_t id, int64_t season_id, const char *name,
                                       const char *date, const char *location, const char *notes,
                                       apl_event *out, apl_error *error);
apl_status apl_event_repository_get(const apl_db_connection *connection, int64_t team_id,
                                    int64_t id, apl_event *out, apl_error *error);
apl_status apl_event_repository_list(const apl_db_connection *connection, int64_t team_id,
                                     apl_event **out, size_t *count, apl_error *error);
void apl_event_repository_free_list(apl_event *events);

#endif
