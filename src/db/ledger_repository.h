#ifndef APL_DB_LEDGER_REPOSITORY_H
#define APL_DB_LEDGER_REPOSITORY_H

#include <stddef.h>
#include <stdint.h>

#include "core/error.h"
#include "db/connection.h"
#include "domain/ledger.h"

/*
 * Immutable team-scoped ledger persistence (work items 4.2/4.3).
 *
 * The repository exposes no update/delete operation. A create writes one entry
 * and returns its stored row (including the derived classification for
 * correction entries). Every read requires an explicit team ID, so an entry
 * belonging to another team is never returned.
 */

/** Insert one entry. `kind` must be an explicit stored code. For reversal and
 *  adjustment entries `corrects_entry_id` must point at an existing entry in
 *  the same team. */
apl_status apl_ledger_repository_create(const apl_db_connection *connection,
                                        const apl_ledger_entry *entry, apl_ledger_entry *out_entry,
                                        apl_error *error);

/** Fetch one entry by team and stable ID. */
apl_status apl_ledger_repository_get(const apl_db_connection *connection, int64_t team_id,
                                     int64_t id, apl_ledger_entry *out_entry, apl_error *error);

/** List entries newest entered-date first, then newest ID, using the optional
 *  filters. The caller releases the array with apl_ledger_repository_free_list. */
apl_status apl_ledger_repository_list(const apl_db_connection *connection,
                                      const apl_ledger_filter *filter,
                                      apl_ledger_entry **out_entries, size_t *out_count,
                                      apl_error *error);

/** Net expense totals by normalized category for a team/date range.
 *
 * Rows include only entries classified as expense, including linked reversals
 * and adjustments. Categories are ordered by category_key. */
apl_status apl_ledger_repository_expense_totals(const apl_db_connection *connection,
                                                int64_t team_id, const char *from_date,
                                                const char *to_date,
                                                apl_ledger_category_total **out_rows,
                                                size_t *out_count, apl_error *error);

/** Return the single reversal for an original entry, or `found` false. */
apl_status apl_ledger_repository_find_reversal(const apl_db_connection *connection, int64_t team_id,
                                               int64_t original_id, apl_ledger_entry *out_entry,
                                               bool *out_found, apl_error *error);

void apl_ledger_repository_free_list(apl_ledger_entry *entries);
void apl_ledger_repository_free_category_totals(apl_ledger_category_total *rows);

#endif /* APL_DB_LEDGER_REPOSITORY_H */
