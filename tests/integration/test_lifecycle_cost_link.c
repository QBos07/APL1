#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "app/app.h"
#include "app/car_service.h"
#include "app/ledger_service.h"
#include "app/lifecycle_service.h"
#include "app/part_type_service.h"
#include "app/part_unit_service.h"
#include "app/team_service.h"
#include "db/activity_repository.h"
#include "db/lifecycle_repository.h"
#include "domain/part.h"
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

static apl_team make_team(apl_app *const app, const char *const name, apl_error *const error)
{
    apl_team team = {};
    cr_assert_eq(apl_team_service_create(app, name, "", &team, error), APL_OK,
                 "team create failed: %s", apl_error_message(error));
    return team;
}

static int64_t make_unit(apl_app *const app, const int64_t team_id, const char *const serial,
                         apl_error *const error)
{
    char type_name[64];
    (void)snprintf(type_name, sizeof(type_name), "Wing %s", serial);
    apl_part_type typed = {};
    cr_assert_eq(
        apl_part_type_service_create(app, team_id, "Aero", type_name, "", "", &typed, error),
        APL_OK);
    apl_part_unit unit = {};
    cr_assert_eq(
        apl_part_unit_service_create(app, team_id, typed.id, serial, "", "", "", &unit, error),
        APL_OK);
    return unit.id;
}

static int64_t make_car(apl_app *const app, const int64_t team_id, const char *const label,
                        apl_error *const error)
{
    apl_car car = {};
    cr_assert_eq(apl_car_service_create(app, team_id, label, "", "", &car, error), APL_OK);
    return car.id;
}

static int64_t set_currency_and_expense(apl_app *const app, const int64_t team_id,
                                        apl_error *const error)
{
    apl_team team = {};
    cr_assert_eq(apl_team_service_set_currency(app, team_id, "EUR", &team, error), APL_OK);
    apl_ledger_entry expense = {};
    cr_assert_eq(apl_ledger_service_post_expense(app, team_id, "1200.50", "2026-01-20", "Parts",
                                                 "replacement wing", &expense, error),
                 APL_OK, "expense failed: %s", apl_error_message(error));
    return expense.id;
}

Test(lifecycle_cost_link, links_replacement_to_expense_without_double_counting)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);
    const int64_t car = make_car(&app, team.id, "SF-26", &err);
    const int64_t old_unit = make_unit(&app, team.id, "FW-100", &err);
    const int64_t new_unit = make_unit(&app, team.id, "FW-200", &err);
    const int64_t expense_id = set_currency_and_expense(&app, team.id, &err);

    cr_assert_eq(apl_lifecycle_service_install(&app, team.id, old_unit, car, "front", "2026-01-01",
                                               "", "", "", "", &(apl_lifecycle_event){}, &err),
                 APL_OK);

    apl_lifecycle_event replaced = {};
    cr_assert_eq(apl_lifecycle_service_replace(&app, team.id, car, "front", old_unit, new_unit,
                                               "2026-01-20", "", "wear", "", "", &replaced, &err),
                 APL_OK);

    /* Replacement starts unlinked. */
    cr_assert_eq(replaced.transaction_id, 0);

    /* Link the expense. */
    apl_lifecycle_event linked = {};
    cr_assert_eq(
        apl_lifecycle_service_set_cost(&app, team.id, replaced.id, expense_id, &linked, &err),
        APL_OK, "cost link failed: %s", apl_error_message(&err));
    cr_assert_eq(linked.transaction_id, expense_id);

    /* The link persists and is readable. */
    apl_lifecycle_event fetched = {};
    cr_assert_eq(apl_lifecycle_service_get(&app, team.id, replaced.id, &fetched, &err), APL_OK);
    cr_assert_eq(fetched.transaction_id, expense_id);

    /* The ledger entry itself is unchanged (still one expense row). */
    apl_ledger_entry expense = {};
    cr_assert_eq(apl_ledger_service_get(&app, team.id, expense_id, &expense, &err), APL_OK);
    cr_assert_eq(expense.classification, APL_LEDGER_KIND_EXPENSE);
    cr_assert_eq(expense.amount.minor_units, -120050);

    /* Unlink clears the link. */
    apl_lifecycle_event unlinked = {};
    cr_assert_eq(apl_lifecycle_service_set_cost(&app, team.id, replaced.id, 0, &unlinked, &err),
                 APL_OK);
    cr_assert_eq(unlinked.transaction_id, 0);

    close_test_app(&app, dir, &err);
}

Test(lifecycle_cost_link, rejects_non_expense_and_cross_team_links)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", &err);
    const apl_team other = make_team(&app, "McLaren", &err);
    const int64_t car = make_car(&app, team.id, "SF-26", &err);
    const int64_t old_unit = make_unit(&app, team.id, "FW-100", &err);
    const int64_t new_unit = make_unit(&app, team.id, "FW-200", &err);

    apl_team team_cur = {};
    cr_assert_eq(apl_team_service_set_currency(&app, team.id, "EUR", &team_cur, &err), APL_OK);
    apl_team other_cur = {};
    cr_assert_eq(apl_team_service_set_currency(&app, other.id, "EUR", &other_cur, &err), APL_OK);
    apl_ledger_entry income = {};
    cr_assert_eq(apl_ledger_service_post_income(&app, team.id, "5000.00", "2026-01-05", "Prize",
                                                "sponsor", &income, &err),
                 APL_OK);
    apl_ledger_entry other_expense = {};
    cr_assert_eq(apl_ledger_service_post_expense(&app, other.id, "900.00", "2026-01-06", "Parts",
                                                 "foreign", &other_expense, &err),
                 APL_OK);

    cr_assert_eq(apl_lifecycle_service_install(&app, team.id, old_unit, car, "front", "2026-01-01",
                                               "", "", "", "", &(apl_lifecycle_event){}, &err),
                 APL_OK);
    apl_lifecycle_event replaced = {};
    cr_assert_eq(apl_lifecycle_service_replace(&app, team.id, car, "front", old_unit, new_unit,
                                               "2026-01-20", "", "", "", "", &replaced, &err),
                 APL_OK);

    /* Income cannot be linked as a replacement cost. */
    apl_lifecycle_event linked = {};
    cr_assert_eq(
        apl_lifecycle_service_set_cost(&app, team.id, replaced.id, income.id, &linked, &err),
        APL_ERR_INVALID_ARGUMENT);

    /* Cross-team expense is rejected. */
    cr_assert_eq(
        apl_lifecycle_service_set_cost(&app, team.id, replaced.id, other_expense.id, &linked, &err),
        APL_ERR_NOT_FOUND);

    /* The replacement's own team expense links fine. */
    apl_ledger_entry expense = {};
    cr_assert_eq(apl_ledger_service_post_expense(&app, team.id, "800.00", "2026-01-07", "Parts",
                                                 "wing", &expense, &err),
                 APL_OK);
    cr_assert_eq(
        apl_lifecycle_service_set_cost(&app, team.id, replaced.id, expense.id, &linked, &err),
        APL_OK);
    cr_assert_eq(linked.transaction_id, expense.id);

    close_test_app(&app, dir, &err);
}