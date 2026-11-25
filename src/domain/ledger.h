#ifndef APL_DOMAIN_LEDGER_H
#define APL_DOMAIN_LEDGER_H

#include <stddef.h>
#include <stdint.h>

#include "core/error.h"
#include "domain/money.h"
#include "domain/reference.h"

/*
 * Team-scoped money ledger values (Phase 4).
 *
 * Entry amounts are signed minor-unit changes to the derived team balance:
 * income is positive, expense is negative, and an opening balance may have
 * either sign (or be zero). Corrections are separate reversal/adjustment
 * entries linked to the original row; posted entries are never edited.
 *
 * Categories are user-entered labels. The display label preserves the user's
 * case, while category_key is the trimmed, single-spaced, lower-case form used
 * for grouping and filtering.
 */

static constexpr size_t APL_LEDGER_CATEGORY_MAX = 128;
static constexpr size_t APL_LEDGER_DESCRIPTION_MAX = 512;

typedef enum apl_ledger_kind {
    APL_LEDGER_KIND_NONE = 0,
    APL_LEDGER_KIND_OPENING_BALANCE,
    APL_LEDGER_KIND_INCOME,
    APL_LEDGER_KIND_EXPENSE,
    APL_LEDGER_KIND_REVERSAL,
    APL_LEDGER_KIND_ADJUSTMENT,
} apl_ledger_kind;

/** Stable storage code (for example "income"), or "" for an invalid kind. */
const char *apl_ledger_kind_code(apl_ledger_kind kind);

/** Display label (for example "Income"), or "Unknown" for an invalid kind. */
const char *apl_ledger_kind_label(apl_ledger_kind kind);

/** Resolve a stable storage code; unknown/missing codes return NONE. */
apl_ledger_kind apl_ledger_kind_from_code(const char *code);

/** True for the kinds a user posts directly (opening balance, income, expense). */
bool apl_ledger_kind_is_postable(apl_ledger_kind kind);

/** One immutable posted ledger entry. `classification` is the original entry's
 *  kind for reversals/adjustments and the entry's own kind otherwise. */
typedef struct apl_ledger_entry {
    int64_t id;
    int64_t team_id;
    apl_money amount;
    char entry_date[APL_REFERENCE_DATE_MAX];
    apl_ledger_kind kind;
    apl_ledger_kind classification;
    char category[APL_LEDGER_CATEGORY_MAX];
    char category_key[APL_LEDGER_CATEGORY_MAX];
    char description[APL_LEDGER_DESCRIPTION_MAX];
    int64_t corrects_entry_id;
    int64_t project_id; /* 0 means not linked to a development project */
    char created_at[APL_RECORD_TIMESTAMP_MAX];
} apl_ledger_entry;

/** A user-entered budget plan. Plans never enter ledger or actual totals. */
typedef struct apl_budget_plan {
    int64_t id;
    int64_t team_id;
    int64_t season_id;
    apl_money amount;
    char start_date[APL_REFERENCE_DATE_MAX];
    char end_date[APL_REFERENCE_DATE_MAX];
    char category[APL_LEDGER_CATEGORY_MAX];
    char category_key[APL_LEDGER_CATEGORY_MAX];
    char notes[APL_REFERENCE_NOTES_MAX];
    char created_at[APL_RECORD_TIMESTAMP_MAX];
    char updated_at[APL_RECORD_TIMESTAMP_MAX];
} apl_budget_plan;

/** Read filters for ledger entries. Empty text/zero fields are unfiltered. */
typedef struct apl_ledger_filter {
    int64_t team_id;
    int64_t project_id;
    apl_ledger_kind kind;
    char from_date[APL_REFERENCE_DATE_MAX];
    char to_date[APL_REFERENCE_DATE_MAX];
    char category_key[APL_LEDGER_CATEGORY_MAX];
} apl_ledger_filter;

/** Read filters for budget plans. Empty text/zero fields are unfiltered. */
typedef struct apl_budget_filter {
    int64_t team_id;
    int64_t season_id;
    char from_date[APL_REFERENCE_DATE_MAX];
    char to_date[APL_REFERENCE_DATE_MAX];
    char category_key[APL_LEDGER_CATEGORY_MAX];
} apl_budget_filter;

/** Derived, checked totals. Income/expense are signed ledger sums. */
typedef struct apl_ledger_totals {
    apl_money balance;
    apl_money income;
    apl_money expense;
    size_t entry_count;
} apl_ledger_totals;

/** Ledger-derived actual cost for one project.
 *
 * `actual_cost` is the positive net expense of linked entries, including
 * linked reversals/adjustments. `entry_count` counts every linked ledger row,
 * not only expenses. There is no independent project cost field.
 */
typedef struct apl_project_cost {
    apl_money actual_cost;
    size_t entry_count;
} apl_project_cost;

/** Per-category net expense contribution (negative for spending). */
typedef struct apl_ledger_category_total {
    apl_money total;
    char category[APL_LEDGER_CATEGORY_MAX];
    char category_key[APL_LEDGER_CATEGORY_MAX];
} apl_ledger_category_total;

/** One planned-versus-actual report row.
 *
 * Missing sides are explicit flags: `has_plan` false means no plan was entered;
 * `has_actual` false means no matching expense was recorded. `variance` is only
 * meaningful (and `over_plan` only set) when both sides are present. */
typedef struct apl_budget_report_row {
    int64_t plan_id;
    int64_t season_id;
    bool has_plan;
    bool has_actual;
    bool over_plan;
    apl_money planned;
    apl_money actual;
    apl_money variance;
    char start_date[APL_REFERENCE_DATE_MAX];
    char end_date[APL_REFERENCE_DATE_MAX];
    char category[APL_LEDGER_CATEGORY_MAX];
    char category_key[APL_LEDGER_CATEGORY_MAX];
    char notes[APL_REFERENCE_NOTES_MAX];
} apl_budget_report_row;

/** Trim and collapse internal whitespace, preserving case for display.
 *
 * `out_display` receives the readable label; `out_key` receives the same text
 * lower-cased for comparison. Both outputs must be APL_LEDGER_CATEGORY_MAX. */
apl_status apl_ledger_category_normalize(const char *text, char *out_display, size_t display_size,
                                         char *out_key, size_t key_size, apl_error *error);

/** Normalize a required transaction date (YYYY-MM-DD). */
apl_status apl_ledger_date_normalize(const char *text, char *out, size_t out_size,
                                     apl_error *error);

/** Normalize a required description. */
apl_status apl_ledger_description_normalize(const char *text, char *out, size_t out_size,
                                            apl_error *error);

#endif /* APL_DOMAIN_LEDGER_H */
