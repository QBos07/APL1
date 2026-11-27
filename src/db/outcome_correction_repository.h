#ifndef APL_DB_OUTCOME_CORRECTION_REPOSITORY_H
#define APL_DB_OUTCOME_CORRECTION_REPOSITORY_H

#include "db/outcome_repository.h"

typedef struct apl_outcome_correction {
    int64_t id, team_id, outcome_id;
    char actor[64];
    char occurred_at[APL_REFERENCE_TIMESTAMP_MAX];
    apl_outcome before;
    apl_outcome after;
} apl_outcome_correction;

/* Append within the same transaction as the outcome update. */
apl_status apl_outcome_correction_append(const apl_db_connection *connection,
                                         const apl_outcome *before, const apl_outcome *after,
                                         apl_error *error);
/* Ordered by insertion id, which breaks ties in timestamps. */
apl_status apl_outcome_correction_list(const apl_db_connection *connection, int64_t team_id,
                                       int64_t outcome_id, apl_outcome_correction **out,
                                       size_t *count, apl_error *error);
void apl_outcome_correction_free_list(apl_outcome_correction *corrections);

#endif
