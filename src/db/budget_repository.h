#ifndef APL_DB_BUDGET_REPOSITORY_H
#define APL_DB_BUDGET_REPOSITORY_H

#include <stddef.h>
#include <stdint.h>

#include "core/error.h"
#include "db/connection.h"
#include "domain/ledger.h"

/*
 * User-entered budget-plan persistence (work item 4.11).
 *
 * Plans are stored separately from ledger entries and never contribute to
 * actual totals. Every operation requires the owning team. The uniqueness
 * policy is one plan per team/start date/end date/normalized category.
 */

/** Insert one plan and return the stored row. */
apl_status apl_budget_repository_create(const apl_db_connection *connection,
                                        const apl_budget_plan *plan, apl_budget_plan *out_plan,
                                        apl_error *error);

/** Fetch one plan by team and stable ID. */
apl_status apl_budget_repository_get(const apl_db_connection *connection, int64_t team_id,
                                     int64_t id, apl_budget_plan *out_plan, apl_error *error);

/** Update one plan's period/category/amount/season/notes (never its team). */
apl_status apl_budget_repository_update(const apl_db_connection *connection, int64_t team_id,
                                        int64_t id, const apl_budget_plan *plan,
                                        apl_budget_plan *out_plan, apl_error *error);

/** Find the plan matching the unique scope, or `found` false. */
apl_status apl_budget_repository_find_scope(const apl_db_connection *connection, int64_t team_id,
                                            const char *start_date, const char *end_date,
                                            const char *category_key, apl_budget_plan *out_plan,
                                            bool *out_found, apl_error *error);

/** List plans ordered by start date, category key, and ID. */
apl_status apl_budget_repository_list(const apl_db_connection *connection,
                                      const apl_budget_filter *filter, apl_budget_plan **out_plans,
                                      size_t *out_count, apl_error *error);

void apl_budget_repository_free_list(apl_budget_plan *plans);

#endif /* APL_DB_BUDGET_REPOSITORY_H */
