#ifndef APL_DB_OUTCOME_REPOSITORY_H
#define APL_DB_OUTCOME_REPOSITORY_H

#include "db/connection.h"
#include "domain/reference.h"

/* Presence flags distinguish SQL NULL from an entered value, including 0 laps. */
typedef struct apl_outcome {
    int64_t id, team_id, event_id, session_id, driver_id;
    bool has_start_position, has_finish_position, has_laps, has_status, has_notes;
    int start_position, finish_position, laps;
    char status[256];
    char notes[4097];
    char created_at[APL_REFERENCE_TIMESTAMP_MAX];
    char updated_at[APL_REFERENCE_TIMESTAMP_MAX];
} apl_outcome;

/* The input's id and timestamps are ignored on create; update changes only
 * values, never the owning team, event, session, or driver. */
apl_status apl_outcome_repository_create(const apl_db_connection *connection,
                                         const apl_outcome *input, apl_outcome *out,
                                         apl_error *error);
apl_status apl_outcome_repository_update(const apl_db_connection *connection, int64_t team_id,
                                         int64_t id, const apl_outcome *values, apl_outcome *out,
                                         apl_error *error);
apl_status apl_outcome_repository_get(const apl_db_connection *connection, int64_t team_id,
                                      int64_t id, apl_outcome *out, apl_error *error);
apl_status apl_outcome_repository_list(const apl_db_connection *connection, int64_t team_id,
                                       int64_t event_id, int64_t session_id, apl_outcome **out,
                                       size_t *count, apl_error *error);
void apl_outcome_repository_free_list(apl_outcome *outcomes);

#endif
