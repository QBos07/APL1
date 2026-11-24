#ifndef APL_DOMAIN_REFERENCE_H
#define APL_DOMAIN_REFERENCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/error.h"

/*
 * User-maintained reference records (Phase 3).
 *
 * Seasons/periods, drivers, team-driver memberships, cars/chassis, and audit
 * activity are entered by the user. This module contains only pure text/date
 * rules so the same validation is used by menus, dialogs, commands, and tests.
 * Optional text is stored as an empty string; optional dates use an empty
 * string to mean "not recorded" rather than inventing a date.
 */

constexpr size_t APL_REFERENCE_LABEL_MAX = 128;
constexpr size_t APL_REFERENCE_SHORT_MAX = 64;
constexpr size_t APL_REFERENCE_NOTES_MAX = 1024;
constexpr size_t APL_REFERENCE_DATE_MAX = 11;
constexpr size_t APL_REFERENCE_TIMESTAMP_MAX = 32;
constexpr size_t APL_ACTIVITY_ACTION_MAX = 64;
constexpr size_t APL_ACTIVITY_ENTITY_MAX = 64;
constexpr size_t APL_ACTIVITY_SUMMARY_MAX = 512;
constexpr size_t APL_ACTIVITY_FIELD_MAX = 64;
constexpr size_t APL_ACTIVITY_VALUE_MAX = 512;

/** A user-created season/reporting period. Dates are ISO YYYY-MM-DD or empty. */
typedef struct apl_season {
    int64_t id;
    int64_t team_id;
    char label[APL_REFERENCE_LABEL_MAX];
    char start_date[APL_REFERENCE_DATE_MAX];
    char end_date[APL_REFERENCE_DATE_MAX];
    char notes[APL_REFERENCE_NOTES_MAX];
    bool archived;
    char created_at[APL_REFERENCE_TIMESTAMP_MAX];
    char updated_at[APL_REFERENCE_TIMESTAMP_MAX];
    char archived_at[APL_REFERENCE_TIMESTAMP_MAX];
} apl_season;

/** A shared driver identity. Team affiliation belongs to memberships. */
typedef struct apl_driver {
    int64_t id;
    char name[APL_REFERENCE_LABEL_MAX];
    char short_name[APL_REFERENCE_SHORT_MAX];
    char notes[APL_REFERENCE_NOTES_MAX];
    bool archived;
    char created_at[APL_REFERENCE_TIMESTAMP_MAX];
    char updated_at[APL_REFERENCE_TIMESTAMP_MAX];
    char archived_at[APL_REFERENCE_TIMESTAMP_MAX];
} apl_driver;

/** One dated driver/team membership period. An empty end date means current. */
typedef struct apl_membership {
    int64_t id;
    int64_t team_id;
    int64_t driver_id;
    char start_date[APL_REFERENCE_DATE_MAX];
    char end_date[APL_REFERENCE_DATE_MAX];
    char role[APL_REFERENCE_SHORT_MAX];
    char notes[APL_REFERENCE_NOTES_MAX];
    char created_at[APL_REFERENCE_TIMESTAMP_MAX];
    char updated_at[APL_REFERENCE_TIMESTAMP_MAX];
} apl_membership;

/** A team-owned car/chassis reference record. */
typedef struct apl_car {
    int64_t id;
    int64_t team_id;
    char label[APL_REFERENCE_LABEL_MAX];
    char chassis[APL_REFERENCE_SHORT_MAX];
    char notes[APL_REFERENCE_NOTES_MAX];
    bool archived;
    char created_at[APL_REFERENCE_TIMESTAMP_MAX];
    char updated_at[APL_REFERENCE_TIMESTAMP_MAX];
    char archived_at[APL_REFERENCE_TIMESTAMP_MAX];
} apl_car;

/** Normalize a required, trimmed label/name. */
apl_status apl_reference_label_normalize(const char *text, const char *label, char *out,
                                         size_t out_size, apl_error *error);

/** Normalize optional trimmed text (null means empty). */
apl_status apl_reference_optional_normalize(const char *text, const char *label, char *out,
                                            size_t out_size, apl_error *error);

/** Normalize an optional ISO date. Empty/null stays empty; invalid dates are
 *  rejected rather than replaced with a placeholder. */
apl_status apl_reference_date_normalize(const char *text, const char *label, char *out,
                                        size_t out_size, apl_error *error);

/** Reject a supplied date range whose start is after its end. Missing dates
 *  are allowed on either or both sides. */
apl_status apl_reference_date_range_validate(const char *start_date, const char *end_date,
                                             const char *label, apl_error *error);

/** Format `days` before now as an ISO date (used for bounded history filters). */
void apl_reference_date_today_minus_days(int days, char *out, size_t out_size);

/** One field-level audit change. A missing old/new value is represented by the
 *  corresponding boolean, not by an empty string. */
typedef struct apl_activity_change {
    char field[APL_ACTIVITY_FIELD_MAX];
    char old_value[APL_ACTIVITY_VALUE_MAX];
    char new_value[APL_ACTIVITY_VALUE_MAX];
    bool old_missing;
    bool new_missing;
} apl_activity_change;

/** One append-only activity event. team_id 0 means no team scope. */
typedef struct apl_activity_event {
    int64_t id;
    int64_t team_id;
    char occurred_at[APL_REFERENCE_TIMESTAMP_MAX];
    char action[APL_ACTIVITY_ACTION_MAX];
    char entity_type[APL_ACTIVITY_ENTITY_MAX];
    int64_t entity_id;
    char summary[APL_ACTIVITY_SUMMARY_MAX];
} apl_activity_event;

/** Read filters for the History view. Empty action/entity/date means unfiltered;
 *  team_id 0 means all teams (an explicit all-team history request). */
typedef struct apl_activity_filter {
    int64_t team_id;
    char action[APL_ACTIVITY_ACTION_MAX];
    char entity_type[APL_ACTIVITY_ENTITY_MAX];
    char from_date[APL_REFERENCE_DATE_MAX];
    char to_date[APL_REFERENCE_DATE_MAX];
} apl_activity_filter;

/** Normalize a required membership start date. */
apl_status apl_membership_start_date_normalize(const char *text, char *out, size_t out_size,
                                               apl_error *error);

/** Normalize optional membership end date and validate the range. */
apl_status apl_membership_dates_normalize(const char *start, const char *end, char *out_start,
                                          size_t out_start_size, char *out_end, size_t out_end_size,
                                          apl_error *error);

#endif /* APL_DOMAIN_REFERENCE_H */
