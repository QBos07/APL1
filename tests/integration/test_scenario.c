#include <criterion/criterion.h>

#include "app/budget_service.h"
#include "app/ledger_service.h"
#include "scenario.h"

Test(scenario_fixture, builds_team_ledger_and_budget_graph)
{
    apl_error error;
    apl_error_init(&error);
    apl_test_scenario scenario = {};
    cr_assert_eq(apl_test_scenario_start(&scenario, &error), APL_OK, "scenario start failed: %s",
                 apl_error_message(&error));
    cr_assert_gt(scenario.team_id, 0);

    apl_ledger_totals totals = {};
    cr_assert_eq(
        apl_ledger_service_totals(&scenario.app, scenario.team_id, "", "", "", &totals, &error),
        APL_OK);
    cr_assert_eq(totals.balance.minor_units, 115000);
    cr_assert_eq(totals.income.minor_units, 25000);
    cr_assert_eq(totals.expense.minor_units, -10000);

    apl_budget_report_row *rows = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_budget_service_report(&scenario.app, scenario.team_id, "", "", "Tyres", &rows,
                                           &count, &error),
                 APL_OK);
    cr_assert_eq(count, 1);
    cr_assert(rows[0].has_plan);
    cr_assert(rows[0].has_actual);
    cr_assert_eq(rows[0].planned.minor_units, 20000);
    cr_assert_eq(rows[0].actual.minor_units, 10000);
    apl_budget_service_free_report(rows);

    cr_assert_eq(apl_test_scenario_close(&scenario, &error), APL_OK);
}
