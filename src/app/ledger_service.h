#ifndef APL_APP_LEDGER_SERVICE_H
#define APL_APP_LEDGER_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include "app/app.h"
#include "core/error.h"
#include "domain/ledger.h"

/*
 * Money ledger use cases (Phase 4).
 *
 * All writes validate against the team's reporting currency, save the entry
 * and its audit event in one transaction, and never edit a posted entry.
 * Corrections append a reversal plus an optional replacement adjustment.
 */

/** Post an opening balance. One opening balance is allowed per team; use a
 *  correction to adjust it later. The amount may be positive, zero, or negative. */
apl_status apl_ledger_service_post_opening(const apl_app *app, int64_t team_id,
                                           const char *amount_text, const char *entry_date,
                                           const char *category, const char *description,
                                           apl_ledger_entry *out_entry, apl_error *error);

/** Post income. The user enters a positive magnitude; the stored amount is
 *  positive. */
apl_status apl_ledger_service_post_income(const apl_app *app, int64_t team_id,
                                          const char *amount_text, const char *entry_date,
                                          const char *category, const char *description,
                                          apl_ledger_entry *out_entry, apl_error *error);

/** Post an expense. The user enters a positive magnitude; the stored amount is
 *  negative. Overspending or a negative resulting balance is never blocked. */
apl_status apl_ledger_service_post_expense(const apl_app *app, int64_t team_id,
                                           const char *amount_text, const char *entry_date,
                                           const char *category, const char *description,
                                           apl_ledger_entry *out_entry, apl_error *error);

/** Fetch one entry for the team. */
apl_status apl_ledger_service_get(const apl_app *app, int64_t team_id, int64_t id,
                                  apl_ledger_entry *out_entry, apl_error *error);

/** List entries through the application boundary. The filter owns the team. */
apl_status apl_ledger_service_list(const apl_app *app, const apl_ledger_filter *filter,
                                   apl_ledger_entry **out_entries, size_t *out_count,
                                   apl_error *error);

/** Derive the current balance and period income/expense totals.
 *
 * Balance sums every entry for the team. Income/expense totals include only
 * entries in the optional date range/category, exclude opening balances, and
 * apply linked reversals/adjustments to the original classification. */
apl_status apl_ledger_service_totals(const apl_app *app, int64_t team_id, const char *from_date,
                                     const char *to_date, const char *category_text,
                                     apl_ledger_totals *out_totals, apl_error *error);

/** Correct one posted entry by appending a reversal and, when
 *  `replacement_amount_text` is nonblank, a replacement adjustment.
 *
 * Reversals retain the original classification for net totals. The original
 * row and its entered values remain unchanged. `out_replacement` is zeroed
 * (id 0) for a pure reversal. */
apl_status apl_ledger_service_correct(const apl_app *app, int64_t team_id, int64_t original_id,
                                      const char *replacement_amount_text,
                                      const char *replacement_date, const char *category,
                                      const char *description, apl_ledger_entry *out_reversal,
                                      apl_ledger_entry *out_replacement, apl_error *error);

void apl_ledger_service_free_list(apl_ledger_entry *entries);

#endif /* APL_APP_LEDGER_SERVICE_H */
