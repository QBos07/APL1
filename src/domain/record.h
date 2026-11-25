#ifndef APL_DOMAIN_RECORD_H
#define APL_DOMAIN_RECORD_H

/*
 * Stable record-kind identities used by audit and history.
 *
 * The code is persisted in activity rows; the label is for display. New
 * record kinds should be appended so old history remains readable.
 */

typedef enum apl_record_kind {
    APL_RECORD_NONE = 0,
    APL_RECORD_TEAM,
    APL_RECORD_SEASON,
    APL_RECORD_DRIVER,
    APL_RECORD_MEMBERSHIP,
    APL_RECORD_CAR,
    APL_RECORD_ACTIVITY,
} apl_record_kind;

/** Stable storage code (for example "team"), or "" for an invalid kind. */
const char *apl_record_kind_code(apl_record_kind kind);

/** Display label (for example "Team"), or "Unknown" for an invalid kind. */
const char *apl_record_kind_label(apl_record_kind kind);

/** Resolve a stable storage code; unknown/missing codes return APL_RECORD_NONE. */
apl_record_kind apl_record_kind_from_code(const char *code);

#endif /* APL_DOMAIN_RECORD_H */
