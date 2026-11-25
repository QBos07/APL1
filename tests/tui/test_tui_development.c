#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "app/app.h"
#include "app/car_service.h"
#include "app/ledger_service.h"
#include "app/project_service.h"
#include "app/season_service.h"
#include "app/team_service.h"
#include "tui/shell_internal.h"
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

static apl_team make_team(apl_app *const app, apl_error *const error)
{
    apl_team team = {};
    cr_assert_eq(apl_team_service_create(app, "Ferrari", "", &team, error), APL_OK);
    cr_assert_eq(apl_team_service_set_currency(app, team.id, "EUR", &team, error), APL_OK);
    return team;
}

Test(tui_development, detail_loads_timeline_and_linked_costs_and_returns_from_money)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, &err);

    apl_project project = {};
    cr_assert_eq(apl_project_service_create(&app, team.id, "Aero", "Body", "Floor", "Open",
                                            "2026-05-01", "", 0, 0, "notes", &project, &err),
                 APL_OK);

    apl_project_update update = {};
    apl_project after = {};
    cr_assert_eq(apl_project_service_log_update(&app, team.id, project.id, "2026-04-01",
                                                "First mock-up", "", "wind tunnel", &update, &after,
                                                &err),
                 APL_OK);

    apl_ledger_entry entry = {};
    cr_assert_eq(apl_ledger_service_post_expense_linked(&app, team.id, "250.00", "2026-04-01",
                                                        "Parts", "Floor", project.id, &entry, &err),
                 APL_OK);

    shell_state state = {};
    state.app = &app;
    state.active_team = team;
    state.active_status = APL_ACTIVE_TEAM_ACTIVE;
    apl_tui_list_init(&state.list, 8);

    cr_assert_eq(load_projects(&state, 0, &err), APL_OK);
    cr_assert_eq(state.project_count, 1);
    cr_assert_eq(state.list.count, 1);

    open_project_detail(&state, project);
    cr_assert_eq(state.section, APL_TUI_SECTION_DEVELOPMENT);
    cr_assert_eq(state.screen, SHELL_SCREEN_PROJECT_DETAIL);
    cr_assert_eq(state.project_update_count, 1);
    cr_assert_str_eq(state.project_updates[0].summary, "First mock-up");
    cr_assert_eq(state.project_entry_count, 1);
    cr_assert_eq(state.project_entries[0].id, entry.id);
    cr_assert_eq(state.project_cost.actual_cost.minor_units, 25000);
    cr_assert_eq(state.project_cost.entry_count, 1);

    handle_project_detail_input(&state, NCKEY_ENTER);
    cr_assert_eq(state.section, APL_TUI_SECTION_MONEY);
    cr_assert_eq(state.screen, SHELL_SCREEN_MONEY_DETAIL);
    cr_assert(state.project_return_pending);
    cr_assert_eq(state.entry_detail.id, entry.id);

    handle_money_detail_input(&state, NCKEY_ESC);
    cr_assert_eq(state.section, APL_TUI_SECTION_DEVELOPMENT);
    cr_assert_eq(state.screen, SHELL_SCREEN_PROJECT_DETAIL);
    cr_assert_not(state.project_return_pending);
    cr_assert_eq(state.project_update_count, 1);
    cr_assert_eq(state.project_entry_count, 1);

    handle_project_detail_input(&state, 'b');
    cr_assert_eq(state.section, APL_TUI_SECTION_MONEY);
    cr_assert_eq(state.screen, SHELL_SCREEN_BUDGET_LIST);

    close_test_app(&app, dir, &err);
}

Test(tui_development, filters_and_empty_detail_states_stay_explicit)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, &err);

    apl_project open = {};
    cr_assert_eq(apl_project_service_create(&app, team.id, "Aero", "Body", "Floor", "Open", "", "",
                                            0, 0, "", &open, &err),
                 APL_OK);
    apl_project paused = {};
    cr_assert_eq(apl_project_service_create(&app, team.id, "Brakes", "Mech", "", "Paused", "", "",
                                            0, 0, "", &paused, &err),
                 APL_OK);

    shell_state state = {};
    state.app = &app;
    state.active_team = team;
    state.active_status = APL_ACTIVE_TEAM_ACTIVE;
    apl_tui_list_init(&state.list, 8);

    (void)snprintf(state.project_status_filter, sizeof(state.project_status_filter), "open");
    cr_assert_eq(load_projects(&state, 0, &err), APL_OK);
    cr_assert_eq(state.project_count, 1);
    cr_assert_str_eq(state.projects[0].title, "Aero");

    state.project_status_filter[0] = '\0';
    cr_assert_eq(load_projects(&state, 0, &err), APL_OK);
    cr_assert_eq(state.project_count, 2);
    cr_assert_str_eq(state.projects[0].title, "Aero");
    cr_assert_str_eq(state.projects[1].title, "Brakes");

    (void)snprintf(state.project_status_filter, sizeof(state.project_status_filter), "open");
    (void)snprintf(state.project_category_filter, sizeof(state.project_category_filter), "body");
    cr_assert_eq(load_projects(&state, 0, &err), APL_OK);
    cr_assert_eq(state.project_count, 1);
    handle_project_list_input(&state, 'c');
    cr_assert_str_empty(state.project_status_filter);
    cr_assert_str_empty(state.project_category_filter);
    cr_assert_eq(state.project_count, 2);

    open_project_detail(&state, paused);
    cr_assert_eq(state.project_update_count, 0);
    cr_assert_eq(state.project_entry_count, 0);
    cr_assert_eq(state.project_cost.actual_cost.minor_units, 0);
    cr_assert_eq(state.project_cost.entry_count, 0);

    close_test_app(&app, dir, &err);
}

Test(tui_development, prefilled_create_form_preserves_values_and_cancel_writes_nothing)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, &err);

    shell_state state = {};
    state.app = &app;
    state.active_team = team;
    state.active_status = APL_ACTIVE_TEAM_ACTIVE;
    apl_tui_list_init(&state.list, 8);

    apl_season season = {};
    cr_assert_eq(apl_season_service_create(&app, team.id, "2026", "", "", "", &season, &err),
                 APL_OK);
    apl_car car = {};
    cr_assert_eq(apl_car_service_create(&app, team.id, "SF-26", "", "", &car, &err), APL_OK);

    open_project_create_form_prefilled(&state, "Aero", "Body", "Floor", "Open", "2026-05-01",
                                       "2026-06-01", "2026", "SF-26", "notes");
    cr_assert(apl_tui_dialog_active(&state.dialog));
    cr_assert_eq(state.dialog_context, SHELL_DIALOG_CONTEXT_PROJECT_CREATE);
    cr_assert_eq(state.dialog_project_id, 0);
    cr_assert_str_eq(apl_tui_dialog_field_at(&state.dialog, 0)->value, "Aero");
    cr_assert_str_eq(apl_tui_dialog_field_at(&state.dialog, 3)->value, "Open");
    cr_assert_str_eq(apl_tui_dialog_field_at(&state.dialog, 6)->value, "2026");
    cr_assert_str_eq(apl_tui_dialog_field_at(&state.dialog, 7)->value, "SF-26");
    cr_assert(apl_tui_dialog_field_at(&state.dialog, 0)->required);

    cr_assert_eq(apl_tui_dialog_dismiss(&state.dialog), APL_TUI_DIALOG_DISMISSED);
    apl_project_filter filter = {.team_id = team.id, .include_archived = true};
    apl_project *projects = nullptr;
    size_t count = 1;
    cr_assert_eq(apl_project_service_list(&app, &filter, &projects, &count, &err), APL_OK);
    cr_assert_eq(count, 0);
    apl_project_service_free_list(projects);

    close_test_app(&app, dir, &err);
}

Test(tui_development, project_form_resolves_season_and_car_labels)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, &err);
    apl_season season = {};
    cr_assert_eq(apl_season_service_create(&app, team.id, "2026", "", "", "", &season, &err),
                 APL_OK);
    apl_car car = {};
    cr_assert_eq(apl_car_service_create(&app, team.id, "SF-26", "", "", &car, &err), APL_OK);

    shell_state state = {};
    state.app = &app;
    state.active_team = team;
    state.active_status = APL_ACTIVE_TEAM_ACTIVE;
    apl_tui_list_init(&state.list, 8);

    cr_assert(perform_project_create(&state, "Aero", "Body", "", "Open", "", "", "2026", "SF-26",
                                     "", &err),
              "create failed: %s", apl_error_message(&err));
    cr_assert_eq(state.project_detail.season_id, season.id);
    cr_assert_eq(state.project_detail.car_id, car.id);

    state.project_detail = (apl_project){};
    cr_assert_not(perform_project_create(&state, "Missing", "", "", "", "", "", "No Such Season",
                                         "", "", &err));
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_SEASON);
    cr_assert_str_eq(apl_error_message(&err),
                     "season 'No Such Season' was not found for this team; leave the field blank "
                     "or use an existing season");

    close_test_app(&app, dir, &err);
}

Test(tui_development, expense_form_links_to_project_by_name)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, &err);
    apl_project project = {};
    cr_assert_eq(apl_project_service_create(&app, team.id, "Aero", "", "", "", "", "", 0, 0, "",
                                            &project, &err),
                 APL_OK);

    shell_state state = {};
    state.app = &app;
    state.active_team = team;
    state.active_status = APL_ACTIVE_TEAM_ACTIVE;
    apl_tui_list_init(&state.list, 8);

    open_money_entry_dialog_for_project(&state, APL_LEDGER_KIND_EXPENSE, "Aero");
    cr_assert(apl_tui_dialog_active(&state.dialog));
    cr_assert_str_eq(apl_tui_dialog_field_at(&state.dialog, 4)->value, "Aero");
    (void)apl_tui_dialog_set_field_value(&state.dialog, 0, "125.00");
    (void)apl_tui_dialog_set_field_value(&state.dialog, 1, "2026-04-01");
    (void)apl_tui_dialog_set_field_value(&state.dialog, 2, "Parts");
    (void)apl_tui_dialog_set_field_value(&state.dialog, 3, "Floor");
    store_money_entry_values(&state);
    cr_assert(perform_money_post(&state, &err), "post failed: %s", apl_error_message(&err));
    cr_assert_eq(state.entry_detail.project_id, project.id);

    apl_ledger_entry *entries = nullptr;
    size_t count = 0;
    cr_assert_eq(
        apl_ledger_service_list_for_project(&app, team.id, project.id, &entries, &count, &err),
        APL_OK);
    cr_assert_eq(count, 1);
    apl_ledger_service_free_list(entries);

    close_test_app(&app, dir, &err);
}

Test(tui_development, project_budget_report_and_archive_are_project_scoped)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error err;
    apl_error_init(&err);
    apl_app app = {};
    open_test_app(&app, dir, sizeof(dir), path, sizeof(path), &err);
    const apl_team team = make_team(&app, &err);
    apl_project project = {};
    cr_assert_eq(apl_project_service_create(&app, team.id, "Aero", "", "", "", "", "", 0, 0, "",
                                            &project, &err),
                 APL_OK);

    apl_budget_plan plan = {};
    cr_assert_eq(apl_budget_service_save(&app, team.id, project.id, "2026-01-01", "2026-01-31",
                                         "300.00", "Tyres", 0, "", &plan, &err),
                 APL_OK);

    shell_state state = {};
    state.app = &app;
    state.active_team = team;
    state.active_status = APL_ACTIVE_TEAM_ACTIVE;
    state.budget_project_filter = project.id;
    apl_tui_list_init(&state.list, 8);

    cr_assert_eq(load_budget_report(&state, &err), APL_OK);
    cr_assert_eq(state.budget_row_count, 1);
    cr_assert_eq(state.budget_rows[0].project_id, project.id);
    cr_assert_eq(state.budget_rows[0].plan_id, plan.id);

    perform_budget_archive(&state, plan.id, &err);
    cr_assert_eq(state.budget_row_count, 0);

    state.include_archived_budgets = true;
    cr_assert_eq(load_budget_report(&state, &err), APL_OK);
    cr_assert_eq(state.budget_row_count, 1);
    cr_assert(state.budget_rows[0].archived);

    close_test_app(&app, dir, &err);
}
