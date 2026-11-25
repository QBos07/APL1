#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "app/app.h"
#include "app/budget_service.h"
#include "app/ledger_service.h"
#include "app/team_service.h"
#include "db/activity_repository.h"
#include "test_support.h"

static void open_test_app(apl_app *const app, char *const dir, const size_t dir_size,
                          char *const path, const size_t path_size, apl_error *const error)
{
    cr_assert_eq(apl_test_temp_dir(dir, dir_size, error), APL_OK);
    const int written = snprintf(path, path_size, "%s/data.sqlite3", dir);
    cr_assert(written > 0 && (size_t)written < path_size);
    const apl_app_config config = {.database_path = path};
    cr_assert_eq(apl_app_start(app, &config, error), APL_OK);
}

static void close_test_app(apl_app *const app, char *const dir, apl_error *const error)
{
    apl_app_stop(app);
    cr_assert_eq(apl_test_remove_temp_dir(dir, error), APL_OK);
}

static apl_team create_team(apl_app *const app, const char *const name, apl_error *const error)
{
    apl_team team = {};
    cr_assert_eq(apl_team_service_create(app, name, "", &team, error), APL_OK);
    cr_assert_eq(apl_team_service_set_currency(app, team.id, "EUR", &team, error), APL_OK);
    return team;
}

static size_t activity_count(apl_app *const app, const int64_t team_id, const char *const action,
                             apl_error *const error)
{
    apl_activity_filter filter = {.team_id = team_id};
    (void)snprintf(filter.action, sizeof(filter.action), "%s", action);
    apl_activity_event *events = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_activity_repository_list(&app->db, &filter, &events, &count, error), APL_OK);
    apl_activity_repository_free_events(events);
    return count;
}

Test(budget_service, plans_are_validated_and_audited_without_touching_the_ledger)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = create_team(&app, "Ferrari", &err);

    apl_budget_plan plan = {};
    cr_assert_eq(apl_budget_service_save(&app, team.id, "2026-01-01", "2026-01-31", "200.00",
                                         "Tyres", 0, "January", &plan, &err),
                 APL_OK, "save failed: %s", apl_error_message(&err));
    cr_assert_gt(plan.id, 0);
    cr_assert_eq(plan.amount.minor_units, 20000);
    cr_assert_str_eq(plan.category, "Tyres");
    cr_assert_str_eq(plan.category_key, "tyres");
    cr_assert_eq(activity_count(&app, team.id, "budget_created", &err), 1);

    /* Saving the same scope updates that plan instead of creating a duplicate. */
    apl_budget_plan updated = {};
    cr_assert_eq(apl_budget_service_save(&app, team.id, "2026-01-01", "2026-01-31", "250.00",
                                         " tyres ", 0, "Revised", &updated, &err),
                 APL_OK);
    cr_assert_eq(updated.id, plan.id);
    cr_assert_eq(updated.amount.minor_units, 25000);
    cr_assert_str_eq(updated.notes, "Revised");
    cr_assert_eq(activity_count(&app, team.id, "budget_created", &err), 1);
    cr_assert_eq(activity_count(&app, team.id, "budget_updated", &err), 1);

    apl_budget_filter filter = {.team_id = team.id};
    apl_budget_plan *plans = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_budget_service_list(&app, &filter, &plans, &count, &err), APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(plans[0].amount.minor_units, 25000);
    apl_budget_service_free_list(plans);

    cr_assert_eq(apl_budget_service_save(&app, team.id, "2026-02-01", "2026-01-31", "10.00",
                                         "Tyres", 0, "", &plan, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_PERIOD);
    cr_assert_eq(apl_budget_service_save(&app, team.id, "2026-02-01", "2026-02-28", "-1.00",
                                         "Tyres", 0, "", &plan, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_AMOUNT);
    cr_assert_eq(apl_budget_service_save(&app, team.id, "2026-02-01", "2026-02-28", "10.00", " ", 0,
                                         "", &plan, &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_CATEGORY);

    apl_ledger_totals totals = {};
    cr_assert_eq(apl_ledger_service_totals(&app, team.id, "", "", "", &totals, &err), APL_OK);
    cr_assert_eq(totals.balance.minor_units, 0);
    cr_assert_eq(totals.entry_count, 0);

    close_test_app(&app, dir, &err);
}

Test(budget_service, report_separates_no_plan_no_actual_and_overspend)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = create_team(&app, "Ferrari", &err);

    apl_budget_plan tyres = {};
    cr_assert_eq(apl_budget_service_save(&app, team.id, "2026-01-01", "2026-01-31", "200.00",
                                         "Tyres", 0, "", &tyres, &err),
                 APL_OK);
    apl_budget_plan fuel = {};
    cr_assert_eq(apl_budget_service_save(&app, team.id, "2026-01-01", "2026-01-31", "100.00",
                                         "Fuel", 0, "", &fuel, &err),
                 APL_OK);

    apl_ledger_entry entry = {};
    cr_assert_eq(apl_ledger_service_post_expense(&app, team.id, "50.00", "2026-01-05", "Tyres",
                                                 "Set one", &entry, &err),
                 APL_OK);
    cr_assert_eq(apl_ledger_service_post_expense(&app, team.id, "100.00", "2026-01-06", "tyres",
                                                 "Set two", &entry, &err),
                 APL_OK);
    cr_assert_eq(apl_ledger_service_post_expense(&app, team.id, "120.00", "2026-01-07", "Fuel",
                                                 "Fuel drums", &entry, &err),
                 APL_OK);
    cr_assert_eq(apl_ledger_service_post_expense(&app, team.id, "40.00", "2025-12-01", "Tyres",
                                                 "Previous period", &entry, &err),
                 APL_OK);
    cr_assert_eq(apl_ledger_service_post_expense(&app, team.id, "30.00", "2026-01-08", "Travel",
                                                 "Unplanned trip", &entry, &err),
                 APL_OK);

    apl_budget_report_row *rows = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_budget_service_report(&app, team.id, "2026-01-01", "2026-01-31", "", &rows,
                                           &count, &err),
                 APL_OK, "report failed: %s", apl_error_message(&err));

    const apl_budget_report_row *tyres_row = nullptr;
    const apl_budget_report_row *fuel_row = nullptr;
    const apl_budget_report_row *travel_row = nullptr;
    for (size_t index = 0; index < count; ++index) {
        if (strcmp(rows[index].category_key, "tyres") == 0) {
            tyres_row = &rows[index];
        } else if (strcmp(rows[index].category_key, "fuel") == 0) {
            fuel_row = &rows[index];
        } else if (strcmp(rows[index].category_key, "travel") == 0) {
            travel_row = &rows[index];
        }
    }
    cr_assert_not_null(tyres_row);
    cr_assert_not_null(fuel_row);
    cr_assert_not_null(travel_row);
    cr_assert_eq(count, 3);

    cr_assert(tyres_row->has_plan);
    cr_assert(tyres_row->has_actual);
    cr_assert_eq(tyres_row->planned.minor_units, 20000);
    cr_assert_eq(tyres_row->actual.minor_units, 15000);
    cr_assert_eq(tyres_row->variance.minor_units, 5000);
    cr_assert_not(tyres_row->over_plan);

    cr_assert(fuel_row->has_plan);
    cr_assert(fuel_row->has_actual);
    cr_assert_eq(fuel_row->actual.minor_units, 12000);
    cr_assert_eq(fuel_row->variance.minor_units, -2000);
    cr_assert(fuel_row->over_plan);

    cr_assert_not(travel_row->has_plan);
    cr_assert(travel_row->has_actual);
    cr_assert_eq(travel_row->actual.minor_units, 3000);

    /* A plan with no matching actual keeps a distinct "no actual" state. */
    apl_budget_service_free_report(rows);
    rows = nullptr;
    count = 0;
    cr_assert_eq(apl_budget_service_report(&app, team.id, "2026-03-01", "2026-03-31", "Tyres",
                                           &rows, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 0);
    apl_budget_service_free_report(rows);

    close_test_app(&app, dir, &err);
}

Test(budget_service, corrections_reduce_actual_spending)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = create_team(&app, "Ferrari", &err);

    apl_budget_plan plan = {};
    cr_assert_eq(apl_budget_service_save(&app, team.id, "2026-01-01", "2026-01-31", "100.00",
                                         "Tyres", 0, "", &plan, &err),
                 APL_OK);
    apl_ledger_entry expense = {};
    cr_assert_eq(apl_ledger_service_post_expense(&app, team.id, "120.00", "2026-01-05", "Tyres",
                                                 "Set", &expense, &err),
                 APL_OK);

    apl_budget_report_row *rows = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_budget_service_report(&app, team.id, "", "", "Tyres", &rows, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 1);
    cr_assert(rows[0].over_plan);
    cr_assert_eq(rows[0].actual.minor_units, 12000);
    apl_budget_service_free_report(rows);

    apl_ledger_entry reversal = {};
    apl_ledger_entry replacement = {};
    cr_assert_eq(apl_ledger_service_correct(&app, team.id, expense.id, "80.00", "2026-01-06",
                                            "Tyres", "Corrected set price", &reversal, &replacement,
                                            &err),
                 APL_OK);

    rows = nullptr;
    count = 0;
    cr_assert_eq(apl_budget_service_report(&app, team.id, "", "", "Tyres", &rows, &count, &err),
                 APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_not(rows[0].over_plan);
    cr_assert_eq(rows[0].actual.minor_units, 8000);
    cr_assert_eq(rows[0].variance.minor_units, 2000);
    apl_budget_service_free_report(rows);

    close_test_app(&app, dir, &err);
}
