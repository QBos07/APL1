#ifndef APL_APP_BUDGET_SERVICE_H
#define APL_APP_BUDGET_SERVICE_H

#include <stddef.h>
#include <stdint.h>

#include "app/app.h"
#include "core/error.h"
#include "domain/ledger.h"

/*
 * User-authored budget plans and planned-versus-actual calculations (4.12/4.13).
 *
 * Plans are never spending: saving or editing one writes only the plan and its
 * audit event. Overspending is always allowed and appears only as a report
 * warning.
 */

/** Validate and save a plan, updating the existing plan when the
 *  team/period/category scope already exists. */
apl_status apl_budget_service_save(const apl_app *app, int64_t team_id, const char *start_date,
                                   const char *end_date, const char *amount_text,
                                   const char *category, int64_t season_id, const char *notes,
                                   apl_budget_plan *out_plan, apl_error *error);

/** Fetch one plan for the team. */
apl_status apl_budget_service_get(const apl_app *app, int64_t team_id, int64_t id,
                                  apl_budget_plan *out_plan, apl_error *error);

/** List plans through the application boundary. */
apl_status apl_budget_service_list(const apl_app *app, const apl_budget_filter *filter,
                                   apl_budget_plan **out_plans, size_t *out_count,
                                   apl_error *error);

/** Build planned-versus-actual rows for an optional date range/category.
 *
 * Each recorded expense contributes to at most one plan row (the first plan
 * matching its normalized category and date range). Expenses outside every
 * plan appear as actual-only rows. `variance` is planned minus actual and is
 * only valid when both sides are present. */
apl_status apl_budget_service_report(const apl_app *app, int64_t team_id, const char *from_date,
                                     const char *to_date, const char *category_text,
                                     apl_budget_report_row **out_rows, size_t *out_count,
                                     apl_error *error);

void apl_budget_service_free_list(apl_budget_plan *plans);
void apl_budget_service_free_report(apl_budget_report_row *rows);

#endif /* APL_APP_BUDGET_SERVICE_H */
