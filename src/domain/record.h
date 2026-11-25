#ifndef APL_DOMAIN_RECORD_H
#define APL_DOMAIN_RECORD_H

#include <stddef.h>
#include <stdint.h>

/*
 * Stable record-kind identities used by audit and history.
 *
 * The code is persisted in activity rows; the label is for display. New
 * record kinds should be appended so old history remains readable.
 */

static constexpr size_t APL_RECORD_TIMESTAMP_MAX = 32;

/** Common lifecycle metadata for persisted records.
 *
 * Used by team, season, driver, car, and membership values. Membership rows do
 * not currently expose an archive action, but keep the same shape so record
 * metadata stays uniform.
 */
typedef struct apl_record_meta {
    bool archived;
    char created_at[APL_RECORD_TIMESTAMP_MAX];
    char updated_at[APL_RECORD_TIMESTAMP_MAX];
    char archived_at[APL_RECORD_TIMESTAMP_MAX];
} apl_record_meta;

typedef enum apl_record_kind {
    APL_RECORD_NONE = 0,
    APL_RECORD_TEAM,
    APL_RECORD_SEASON,
    APL_RECORD_DRIVER,
    APL_RECORD_MEMBERSHIP,
    APL_RECORD_CAR,
    APL_RECORD_ACTIVITY,
    APL_RECORD_LEDGER_ENTRY,
    APL_RECORD_BUDGET_PLAN,
} apl_record_kind;

/** Stable storage code (for example "team"), or "" for an invalid kind. */
const char *apl_record_kind_code(apl_record_kind kind);

/** Display label (for example "Team"), or "Unknown" for an invalid kind. */
const char *apl_record_kind_label(apl_record_kind kind);

/** Resolve a stable storage code; unknown/missing codes return APL_RECORD_NONE. */
apl_record_kind apl_record_kind_from_code(const char *code);

/** Typed reference to one persisted record. Used by audit/history routing so a
 *  row can identify both its entity kind and stable ID without a polymorphic
 *  string key. An ID of 0 means no record. */
typedef struct apl_record_ref {
    apl_record_kind kind;
    int64_t id;
} apl_record_ref;

/** Build a typed reference from a kind and stable ID. */
apl_record_ref apl_record_ref_make(apl_record_kind kind, int64_t id);

/** True when the reference names a known kind and a positive ID. */
bool apl_record_ref_valid(const apl_record_ref *ref);

/** True when both references name the same kind and ID. */
bool apl_record_ref_equal(const apl_record_ref *left, const apl_record_ref *right);

#endif /* APL_DOMAIN_RECORD_H */
