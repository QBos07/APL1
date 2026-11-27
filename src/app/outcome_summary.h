#ifndef APL_APP_OUTCOME_SUMMARY_H
#define APL_APP_OUTCOME_SUMMARY_H

#include "app/app.h"
#include "domain/reference.h"

/* Zero IDs and empty dates mean no filter. Dates are inclusive event dates. */
typedef struct apl_outcome_summary_filter {
    int64_t team_id, event_id, session_id, season_id;
    char from_date[APL_REFERENCE_DATE_MAX];
    char to_date[APL_REFERENCE_DATE_MAX];
} apl_outcome_summary_filter;

typedef struct apl_outcome_summary {
    int64_t outcome_count;
    int64_t laps_recorded_count;
    int64_t laps_missing_count;
    bool has_laps_total;
    int64_t laps_total;
} apl_outcome_summary;

/* Sums only explicitly recorded laps. A dataset with no recorded lap counts
 * has no sum, rather than a fabricated zero. No ranking is calculated. */
apl_status apl_outcome_summary_query(const apl_app *app, const apl_outcome_summary_filter *filter,
                                     apl_outcome_summary *out, apl_error *error);

#endif
