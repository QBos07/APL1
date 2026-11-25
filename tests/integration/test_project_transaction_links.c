#include <stdio.h>

#include <criterion/criterion.h>

#include "app/app.h"
#include "app/ledger_service.h"
#include "app/project_service.h"
#include "app/team_service.h"
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

static apl_team make_team(apl_app *const app, const char *const name, const char *const currency,
                          apl_error *const error)
{
    apl_team team = {};
    cr_assert_eq(apl_team_service_create(app, name, "", &team, error), APL_OK,
                 "team create failed: %s", apl_error_message(error));
    cr_assert_eq(apl_team_service_set_currency(app, team.id, currency, &team, error), APL_OK,
                 "currency failed: %s", apl_error_message(error));
    return team;
}

static apl_project make_project(apl_app *const app, const int64_t team_id, const char *const title,
                                apl_error *const error)
{
    apl_project project = {};
    cr_assert_eq(apl_project_service_create(app, team_id, title, "", "", "", "", "", 0, 0, "",
                                            &project, error),
                 APL_OK, "project create failed: %s", apl_error_message(error));
    return project;
}

static size_t ledger_count(apl_app *const app, const int64_t team_id, apl_error *const error)
{
    apl_ledger_filter filter = {.team_id = team_id};
    apl_ledger_entry *entries = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_ledger_service_list(app, &filter, &entries, &count, error), APL_OK);
    apl_ledger_service_free_list(entries);
    return count;
}

Test(project_transaction_links, links_expenses_and_derives_cost_without_double_counting)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", "EUR", &err);
    const apl_project project = make_project(&app, team.id, "Aero", &err);

    apl_ledger_entry linked = {};
    cr_assert_eq(apl_ledger_service_post_expense_linked(&app, team.id, "250.00", "2026-04-01",
                                                        "Parts", "Floor", project.id, &linked,
                                                        &err),
                 APL_OK, "linked expense failed: %s", apl_error_message(&err));
    cr_assert_eq(linked.project_id, project.id);
    cr_assert_eq(linked.amount.minor_units, -25000);

    apl_ledger_entry unlinked = {};
    cr_assert_eq(apl_ledger_service_post_expense(&app, team.id, "100.00", "2026-04-02", "Travel",
                                                 "Test trip", &unlinked, &err),
                 APL_OK);
    cr_assert_eq(unlinked.project_id, 0);

    apl_ledger_entry *entries = nullptr;
    size_t count = 0;
    cr_assert_eq(
        apl_ledger_service_list_for_project(&app, team.id, project.id, &entries, &count, &err),
        APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(entries[0].id, linked.id);
    cr_assert_eq(entries[0].project_id, project.id);
    apl_ledger_service_free_list(entries);

    apl_project_cost cost = {};
    cr_assert_eq(apl_ledger_service_project_cost(&app, team.id, project.id, &cost, &err), APL_OK);
    cr_assert_eq(cost.actual_cost.minor_units, 25000);
    cr_assert_str_eq(cost.actual_cost.currency, "EUR");
    cr_assert_eq(cost.entry_count, 1);

    apl_ledger_totals totals = {};
    cr_assert_eq(apl_ledger_service_totals(&app, team.id, "", "", "", &totals, &err), APL_OK);
    cr_assert_eq(totals.expense.minor_units, -35000);
    cr_assert_eq(totals.balance.minor_units, -35000);
    cr_assert_eq(ledger_count(&app, team.id, &err), 2);

    close_test_app(&app, dir, &err);
}

Test(project_transaction_links, corrections_preserve_the_project_link_and_net_cost)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, "Ferrari", "EUR", &err);
    const apl_project project = make_project(&app, team.id, "Aero", &err);

    apl_ledger_entry original = {};
    cr_assert_eq(apl_ledger_service_post_expense_linked(&app, team.id, "250.00", "2026-04-01",
                                                        "Parts", "Floor", project.id, &original,
                                                        &err),
                 APL_OK);

    apl_ledger_entry reversal = {};
    apl_ledger_entry replacement = {};
    cr_assert_eq(apl_ledger_service_correct(&app, team.id, original.id, "200.00", "2026-04-02",
                                            "Parts", "Corrected floor", &reversal, &replacement,
                                            &err),
                 APL_OK, "correction failed: %s", apl_error_message(&err));
    cr_assert_eq(reversal.project_id, project.id);
    cr_assert_eq(replacement.project_id, project.id);
    cr_assert_eq(reversal.classification, APL_LEDGER_KIND_EXPENSE);
    cr_assert_eq(replacement.classification, APL_LEDGER_KIND_EXPENSE);

    apl_ledger_entry *entries = nullptr;
    size_t count = 0;
    cr_assert_eq(
        apl_ledger_service_list_for_project(&app, team.id, project.id, &entries, &count, &err),
        APL_OK);
    cr_assert_eq(count, 3);
    for (size_t index = 0; index < count; ++index) {
        cr_assert_eq(entries[index].project_id, project.id);
    }
    apl_ledger_service_free_list(entries);

    apl_project_cost cost = {};
    cr_assert_eq(apl_ledger_service_project_cost(&app, team.id, project.id, &cost, &err), APL_OK);
    cr_assert_eq(cost.actual_cost.minor_units, 20000);
    cr_assert_eq(cost.entry_count, 3);

    apl_ledger_totals totals = {};
    cr_assert_eq(apl_ledger_service_totals(&app, team.id, "", "", "", &totals, &err), APL_OK);
    cr_assert_eq(totals.expense.minor_units, -20000);
    cr_assert_eq(totals.balance.minor_units, -20000);
    cr_assert_eq(ledger_count(&app, team.id, &err), 3);

    close_test_app(&app, dir, &err);
}

Test(project_transaction_links, rejects_cross_team_and_archived_project_links)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team ferrari = make_team(&app, "Ferrari", "EUR", &err);
    const apl_team mclaren = make_team(&app, "McLaren", "GBP", &err);
    const apl_project project = make_project(&app, ferrari.id, "Aero", &err);
    const apl_project other = make_project(&app, mclaren.id, "Other", &err);

    apl_ledger_entry entry = {};
    cr_assert_eq(apl_ledger_service_post_expense_linked(&app, ferrari.id, "10.00", "2026-04-01",
                                                        "Parts", "Cross", other.id, &entry, &err),
                 APL_ERR_NOT_FOUND);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_PROJECT);
    cr_assert_eq(ledger_count(&app, ferrari.id, &err), 0);

    apl_project archived = {};
    cr_assert_eq(apl_project_service_archive(&app, ferrari.id, project.id, &archived, &err),
                 APL_OK);
    cr_assert_eq(apl_ledger_service_post_expense_linked(&app, ferrari.id, "10.00", "2026-04-01",
                                                        "Parts", "Archived", project.id, &entry,
                                                        &err),
                 APL_ERR_CONFLICT);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_PROJECT);
    cr_assert_eq(ledger_count(&app, ferrari.id, &err), 0);

    /* A link recorded before archival stays readable and costable. */
    apl_project active = make_project(&app, ferrari.id, "Brakes", &err);
    cr_assert_eq(apl_ledger_service_post_expense_linked(&app, ferrari.id, "40.00", "2026-04-01",
                                                        "Parts", "Pads", active.id, &entry, &err),
                 APL_OK);
    cr_assert_eq(apl_project_service_archive(&app, ferrari.id, active.id, &archived, &err), APL_OK);
    apl_project_cost cost = {};
    cr_assert_eq(apl_ledger_service_project_cost(&app, ferrari.id, active.id, &cost, &err), APL_OK);
    cr_assert_eq(cost.actual_cost.minor_units, 4000);

    close_test_app(&app, dir, &err);
}
