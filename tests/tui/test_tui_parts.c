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
#include "tui/shell_internal.h"
#include "test_support.h"

static void open_parts_test_app(apl_app *const app, char *const dir, const size_t dir_size,
                                char *const path, const size_t path_size, apl_error *const error)
{
    cr_assert_eq(apl_test_temp_dir(dir, dir_size, error), APL_OK);
    const int written = snprintf(path, path_size, "%s/data.sqlite3", dir);
    cr_assert(written > 0 && (size_t)written < path_size);
    const apl_app_config config = {.database_path = path};
    cr_assert_eq(apl_app_start(app, &config, error), APL_OK);
}

static void close_parts_test_app(apl_app *const app, char *const dir, apl_error *const error)
{
    apl_app_stop(app);
    cr_assert_eq(apl_test_remove_temp_dir(dir, error), APL_OK);
}

Test(tui_parts, filters_compose_against_lifecycle_and_user_entered_part_details)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error error;
    apl_error_init(&error);
    apl_app app = {};
    open_parts_test_app(&app, dir, sizeof(dir), path, sizeof(path), &error);

    apl_team team = {};
    cr_assert_eq(apl_team_service_create(&app, "Filter Team", "", &team, &error), APL_OK);
    apl_car car = {};
    apl_car other_car = {};
    cr_assert_eq(apl_car_service_create(&app, team.id, "SF-26", "", "", &car, &error), APL_OK);
    cr_assert_eq(apl_car_service_create(&app, team.id, "SF-25", "", "", &other_car, &error),
                 APL_OK);

    apl_part_type aero = {};
    apl_part_type wheel = {};
    cr_assert_eq(apl_part_type_service_create(&app, team.id, "Aero", "Front Wing", "CFK", "body",
                                              &aero, &error),
                 APL_OK);
    cr_assert_eq(apl_part_type_service_create(&app, team.id, "Suspension", "Wheel", "", "rim",
                                              &wheel, &error),
                 APL_OK);

    apl_part_unit front_wing = {};
    apl_part_unit rear_wing = {};
    apl_part_unit wheel_unit = {};
    cr_assert_eq(apl_part_unit_service_create(&app, team.id, aero.id, "FW-101", "P-101", "New",
                                              "front notes", &front_wing, &error),
                 APL_OK);
    cr_assert_eq(apl_part_unit_service_create(&app, team.id, aero.id, "FW-202", "P-202", "Used",
                                              "rear notes", &rear_wing, &error),
                 APL_OK);
    cr_assert_eq(apl_part_unit_service_create(&app, team.id, wheel.id, "WH-303", "P-303", "New",
                                              "wheel notes", &wheel_unit, &error),
                 APL_OK);

    apl_lifecycle_event event = {};
    cr_assert_eq(apl_lifecycle_service_install(&app, team.id, front_wing.id, car.id, "front",
                                               "2026-01-10", "", "", "", "", &event, &error),
                 APL_OK);
    cr_assert_eq(apl_lifecycle_service_install(&app, team.id, rear_wing.id, car.id, "rear",
                                               "2026-02-10", "", "", "", "", &event, &error),
                 APL_OK);
    cr_assert_eq(apl_lifecycle_service_install(&app, team.id, wheel_unit.id, other_car.id, "front",
                                               "2026-01-12", "", "", "", "", &event, &error),
                 APL_OK);

    shell_state state = {};
    state.app = &app;
    state.active_team = team;
    state.active_status = APL_ACTIVE_TEAM_ACTIVE;
    state.section = APL_TUI_SECTION_PARTS;
    state.screen = SHELL_SCREEN_PART_UNIT_LIST;
    apl_tui_list_init(&state.list, 8);
    cr_assert_eq(load_part_units(&state, 0, &error), APL_OK);
    cr_assert_eq(state.part_unit_count, 3);

    cr_assert(apply_parts_filter(&state, "INSTALL", "SF-26", "front", "2026-01-01", "2026-01-31",
                                 "fw-101", "aero", &error));
    cr_assert_eq(state.part_unit_count, 1);
    cr_assert_eq(state.part_units[0].id, front_wing.id);

    cr_assert(apply_parts_filter(&state, "replace", "", "", "", "", "", "", &error));
    cr_assert_eq(state.part_unit_count, 0);

    cr_assert(apply_parts_filter(&state, "", "", "", "", "", "rim", "wheel", &error));
    cr_assert_eq(state.part_unit_count, 1);
    cr_assert_eq(state.part_units[0].id, wheel_unit.id);

    cr_assert(apply_parts_filter(&state, "install", "SF-26", "front", "2026-01-01", "2026-01-31",
                                 "", "aero", &error));
    state.screen = SHELL_SCREEN_PARTS_FITMENT;
    cr_assert(reload_parts_with_filters(&state, &error));
    cr_assert_eq(state.fitment_count, 1);
    cr_assert_eq(state.fitment_rows[0].unit_id, front_wing.id);

    apl_part_unit_service_free_list(state.part_units);
    apl_lifecycle_service_free_fitment(state.fitment_rows);
    close_parts_test_app(&app, dir, &error);
}

Test(tui_parts, fitment_view_distinguishes_unknown_from_recorded_empty_slot)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error error;
    apl_error_init(&error);
    apl_app app = {};
    open_parts_test_app(&app, dir, sizeof(dir), path, sizeof(path), &error);

    apl_team team = {};
    cr_assert_eq(apl_team_service_create(&app, "Fitment Team", "", &team, &error), APL_OK);
    apl_car car = {};
    cr_assert_eq(apl_car_service_create(&app, team.id, "SF-26", "", "", &car, &error), APL_OK);
    apl_part_type type = {};
    cr_assert_eq(apl_part_type_service_create(&app, team.id, "Aero", "Wing", "", "", &type, &error),
                 APL_OK);
    apl_part_unit unit = {};
    cr_assert_eq(
        apl_part_unit_service_create(&app, team.id, type.id, "FW-1", "", "", "", &unit, &error),
        APL_OK);

    shell_state state = {};
    state.app = &app;
    state.active_team = team;
    state.active_status = APL_ACTIVE_TEAM_ACTIVE;
    state.screen = SHELL_SCREEN_PARTS_FITMENT;
    apl_tui_list_init(&state.list, 8);
    cr_assert_eq(load_parts_fitment(&state, &error), APL_OK);
    cr_assert_eq(state.fitment_count, 0);

    apl_lifecycle_event event = {};
    cr_assert_eq(apl_lifecycle_service_install(&app, team.id, unit.id, car.id, "front",
                                               "2026-01-01", "", "", "", "", &event, &error),
                 APL_OK);
    cr_assert_eq(load_parts_fitment(&state, &error), APL_OK);
    cr_assert_eq(state.fitment_count, 1);
    cr_assert_eq(state.fitment_rows[0].unit_id, unit.id);
    cr_assert_eq(load_part_unit_detail(&state, unit.id, &error), APL_OK);
    cr_assert(state.part_unit_fitment_known);
    cr_assert(state.part_unit_is_fitted);

    cr_assert_eq(apl_lifecycle_service_remove(&app, team.id, unit.id, car.id, "front", "2026-01-02",
                                              "", "", "", "", &event, &error),
                 APL_OK);
    cr_assert_eq(load_parts_fitment(&state, &error), APL_OK);
    cr_assert_eq(state.fitment_count, 1);
    cr_assert_eq(state.fitment_rows[0].unit_id, 0);
    cr_assert_eq(state.fitment_rows[0].car_id, car.id);
    cr_assert_eq(load_part_unit_detail(&state, unit.id, &error), APL_OK);
    cr_assert(state.part_unit_fitment_known);
    cr_assert_not(state.part_unit_is_fitted);

    apl_lifecycle_service_free_fitment(state.fitment_rows);
    apl_lifecycle_service_free_history(state.unit_events);
    close_parts_test_app(&app, dir, &error);
}

Test(tui_parts, lifecycle_form_requires_review_and_cancel_writes_nothing)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error error;
    apl_error_init(&error);
    apl_app app = {};
    open_parts_test_app(&app, dir, sizeof(dir), path, sizeof(path), &error);

    apl_team team = {};
    cr_assert_eq(apl_team_service_create(&app, "Review Team", "", &team, &error), APL_OK);
    apl_part_type type = {};
    cr_assert_eq(apl_part_type_service_create(&app, team.id, "Aero", "Wing", "", "", &type, &error),
                 APL_OK);
    apl_part_unit unit = {};
    cr_assert_eq(
        apl_part_unit_service_create(&app, team.id, type.id, "", "", "", "", &unit, &error),
        APL_OK);

    shell_state state = {};
    state.app = &app;
    state.active_team = team;
    state.active_status = APL_ACTIVE_TEAM_ACTIVE;
    state.section = APL_TUI_SECTION_PARTS;
    state.screen = SHELL_SCREEN_PART_UNIT_DETAIL;
    state.part_unit_detail = unit;
    apl_tui_list_init(&state.list, 8);

    open_lifecycle_dialog(&state, APL_LIFECYCLE_REPAIR);
    apl_tui_dialog_set_field_value(&state.dialog, 3, "2026-03-01");
    ncinput input = {.id = NCKEY_ENTER};
    handle_dialog_input(&state, &input);
    cr_assert_eq(state.dialog.kind, APL_TUI_DIALOG_CONFIRM);
    cr_assert(strstr(state.dialog.message, "without changing current fitment") != nullptr);
    cr_assert_eq(state.pending, SHELL_PENDING_APPLY_LIFECYCLE);

    input.id = NCKEY_ESC;
    handle_dialog_input(&state, &input);
    apl_lifecycle_history_entry *history = nullptr;
    size_t count = 0;
    cr_assert_eq(apl_lifecycle_service_list_unit(&app, team.id, unit.id, &history, &count, &error),
                 APL_OK);
    cr_assert_eq(count, 0);
    apl_lifecycle_service_free_history(history);

    open_lifecycle_dialog(&state, APL_LIFECYCLE_REPAIR);
    apl_tui_dialog_set_field_value(&state.dialog, 3, "2026-03-01");
    input.id = NCKEY_ENTER;
    handle_dialog_input(&state, &input);
    state.dialog.focus = 1;
    handle_dialog_input(&state, &input);
    cr_assert_eq(state.dialog.kind, APL_TUI_DIALOG_NONE);
    cr_assert_eq(apl_lifecycle_service_list_unit(&app, team.id, unit.id, &history, &count, &error),
                 APL_OK);
    cr_assert_eq(count, 1);
    cr_assert_eq(history[0].event.action, APL_LIFECYCLE_REPAIR);
    apl_lifecycle_service_free_history(history);

    apl_part_unit_service_free_list(state.part_units);
    apl_lifecycle_service_free_fitment(state.fitment_rows);
    apl_lifecycle_service_free_history(state.unit_events);
    close_parts_test_app(&app, dir, &error);
}

Test(tui_parts, replacement_review_links_an_existing_expense)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error error;
    apl_error_init(&error);
    apl_app app = {};
    open_parts_test_app(&app, dir, sizeof(dir), path, sizeof(path), &error);

    apl_team team = {};
    cr_assert_eq(apl_team_service_create(&app, "Replacement Team", "", &team, &error), APL_OK);
    cr_assert_eq(apl_team_service_set_currency(&app, team.id, "EUR", &team, &error), APL_OK);
    apl_car car = {};
    cr_assert_eq(apl_car_service_create(&app, team.id, "SF-26", "", "", &car, &error), APL_OK);
    apl_part_type type = {};
    cr_assert_eq(apl_part_type_service_create(&app, team.id, "Aero", "Wing", "", "", &type, &error),
                 APL_OK);
    apl_part_unit old_unit = {};
    apl_part_unit new_unit = {};
    cr_assert_eq(
        apl_part_unit_service_create(&app, team.id, type.id, "OLD", "", "", "", &old_unit, &error),
        APL_OK);
    cr_assert_eq(
        apl_part_unit_service_create(&app, team.id, type.id, "NEW", "", "", "", &new_unit, &error),
        APL_OK);
    apl_lifecycle_event event = {};
    cr_assert_eq(apl_lifecycle_service_install(&app, team.id, old_unit.id, car.id, "front",
                                               "2026-01-01", "", "", "", "", &event, &error),
                 APL_OK);
    apl_ledger_entry expense = {};
    cr_assert_eq(apl_ledger_service_post_expense(&app, team.id, "125.00", "2026-01-02", "Parts",
                                                 "wing replacement", &expense, &error),
                 APL_OK);

    shell_state state = {};
    state.app = &app;
    state.active_team = team;
    state.active_status = APL_ACTIVE_TEAM_ACTIVE;
    state.section = APL_TUI_SECTION_PARTS;
    state.screen = SHELL_SCREEN_PART_UNIT_DETAIL;
    apl_tui_list_init(&state.list, 8);
    open_part_unit_detail(&state, old_unit);
    cr_assert(state.part_unit_fitment_known);
    cr_assert(state.part_unit_is_fitted);
    cr_assert_str_eq(state.part_unit_fitment.position, "front");

    open_replace_dialog_direct(&state);
    char new_id[32];
    char expense_id[32];
    (void)snprintf(new_id, sizeof(new_id), "%lld", (long long)new_unit.id);
    (void)snprintf(expense_id, sizeof(expense_id), "%lld", (long long)expense.id);
    apl_tui_dialog_set_field_value(&state.dialog, 1, new_id);
    apl_tui_dialog_set_field_value(&state.dialog, 2, "SF-26");
    apl_tui_dialog_set_field_value(&state.dialog, 3, "front");
    apl_tui_dialog_set_field_value(&state.dialog, 4, "2026-02-01");
    apl_tui_dialog_set_field_value(&state.dialog, 9, expense_id);
    ncinput input = {.id = NCKEY_ENTER};
    handle_dialog_input(&state, &input);
    cr_assert_eq(state.dialog.kind, APL_TUI_DIALOG_CONFIRM);
    cr_assert(strstr(state.dialog.message, expense_id) != nullptr);
    input.id = NCKEY_ESC;
    handle_dialog_input(&state, &input);

    apl_fitment_row *fitment = nullptr;
    size_t fitment_count = 0;
    cr_assert_eq(
        apl_lifecycle_service_current_fitment(&app, team.id, &fitment, &fitment_count, &error),
        APL_OK);
    cr_assert_eq(fitment_count, 1);
    cr_assert_eq(fitment[0].unit_id, old_unit.id);
    apl_lifecycle_service_free_fitment(fitment);

    open_replace_dialog_direct(&state);
    apl_tui_dialog_set_field_value(&state.dialog, 1, new_id);
    apl_tui_dialog_set_field_value(&state.dialog, 2, "SF-26");
    apl_tui_dialog_set_field_value(&state.dialog, 3, "front");
    apl_tui_dialog_set_field_value(&state.dialog, 4, "2026-02-01");
    apl_tui_dialog_set_field_value(&state.dialog, 9, expense_id);
    input.id = NCKEY_ENTER;
    handle_dialog_input(&state, &input);
    state.dialog.focus = 1;
    handle_dialog_input(&state, &input);

    apl_lifecycle_history_entry *history = nullptr;
    size_t history_count = 0;
    cr_assert_eq(apl_lifecycle_service_list_unit(&app, team.id, old_unit.id, &history,
                                                 &history_count, &error),
                 APL_OK);
    cr_assert_eq(history_count, 2);
    cr_assert_eq(history[1].event.action, APL_LIFECYCLE_REPLACE);
    cr_assert_eq(history[1].event.transaction_id, expense.id);
    apl_lifecycle_service_free_history(history);
    cr_assert(state.part_unit_fitment_known);
    cr_assert(state.part_unit_is_fitted);

    handle_part_unit_detail_input(&state, 'c');
    cr_assert_eq(state.section, APL_TUI_SECTION_MONEY);
    cr_assert_eq(state.screen, SHELL_SCREEN_MONEY_DETAIL);
    cr_assert(state.parts_return_pending);
    cr_assert_eq(state.entry_detail.id, expense.id);
    handle_money_detail_input(&state, NCKEY_ESC);
    cr_assert_eq(state.section, APL_TUI_SECTION_PARTS);
    cr_assert_eq(state.screen, SHELL_SCREEN_PART_UNIT_DETAIL);
    cr_assert_not(state.parts_return_pending);
    open_part_unit_detail(&state, old_unit);
    cr_assert(state.part_unit_fitment_known);
    cr_assert_not(state.part_unit_is_fitted);

    apl_part_unit_service_free_list(state.part_units);
    apl_lifecycle_service_free_fitment(state.fitment_rows);
    apl_lifecycle_service_free_history(state.unit_events);
    apl_ledger_service_free_list(state.entries);
    free(state.entry_events);
    close_parts_test_app(&app, dir, &error);
}

Test(tui_parts, register_forms_create_edit_and_confirm_archive_without_seed_data)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error error;
    apl_error_init(&error);
    apl_app app = {};
    open_parts_test_app(&app, dir, sizeof(dir), path, sizeof(path), &error);

    apl_team team = {};
    cr_assert_eq(apl_team_service_create(&app, "Register Team", "", &team, &error), APL_OK);
    shell_state state = {};
    state.app = &app;
    state.active_team = team;
    state.active_status = APL_ACTIVE_TEAM_ACTIVE;
    state.section = APL_TUI_SECTION_PARTS;
    state.screen = SHELL_SCREEN_PART_TYPE_LIST;
    apl_tui_list_init(&state.list, 8);

    open_part_type_dialog(&state, false);
    apl_tui_dialog_set_field_value(&state.dialog, 0, "Aero");
    apl_tui_dialog_set_field_value(&state.dialog, 1, "Front Wing");
    apl_tui_dialog_set_field_value(&state.dialog, 2, "Composite");
    ncinput input = {.id = NCKEY_ENTER};
    handle_dialog_input(&state, &input);
    cr_assert_eq(state.dialog.kind, APL_TUI_DIALOG_NONE);
    cr_assert_eq(state.part_type_count, 1);
    const apl_part_type type = state.part_types[0];

    handle_part_type_list_input(&state, NCKEY_ENTER);
    handle_part_type_detail_input(&state, 'n');
    cr_assert_eq(state.dialog_context, SHELL_DIALOG_CONTEXT_PART_UNIT_CREATE);
    char expected_type[32];
    (void)snprintf(expected_type, sizeof(expected_type), "%lld", (long long)type.id);
    cr_assert_str_eq(apl_tui_dialog_field_at(&state.dialog, 0)->value, expected_type);
    apl_tui_dialog_set_field_value(&state.dialog, 1, "");
    apl_tui_dialog_set_field_value(&state.dialog, 2, "");
    handle_dialog_input(&state, &input);
    cr_assert_eq(state.dialog.kind, APL_TUI_DIALOG_NONE);
    cr_assert_eq(state.screen, SHELL_SCREEN_PART_UNIT_DETAIL);
    cr_assert_eq(state.part_unit_detail.type_id, type.id);
    cr_assert_str_eq(state.part_unit_detail.serial_number, "");
    cr_assert_str_eq(state.part_unit_detail.part_number, "");

    handle_part_unit_detail_input(&state, 'e');
    apl_tui_dialog_set_field_value(&state.dialog, 4, "updated notes");
    handle_dialog_input(&state, &input);
    cr_assert_str_eq(state.part_unit_detail.notes, "updated notes");

    handle_part_unit_detail_input(&state, 'x');
    cr_assert_eq(state.dialog.kind, APL_TUI_DIALOG_CONFIRM);
    input.id = NCKEY_ESC;
    handle_dialog_input(&state, &input);
    apl_part_unit fetched_unit = {};
    cr_assert_eq(
        apl_part_unit_service_get(&app, team.id, state.dialog_part_unit_id, &fetched_unit, &error),
        APL_OK);
    cr_assert_not(fetched_unit.meta.archived);

    handle_part_unit_detail_input(&state, 'x');
    state.dialog.focus = 1;
    input.id = NCKEY_ENTER;
    handle_dialog_input(&state, &input);
    cr_assert_eq(apl_part_unit_service_get(&app, team.id, fetched_unit.id, &fetched_unit, &error),
                 APL_OK);
    cr_assert(fetched_unit.meta.archived);

    open_part_type_detail(&state, type);
    handle_part_type_detail_input(&state, 'x');
    state.dialog.focus = 1;
    handle_dialog_input(&state, &input);
    apl_part_type fetched_type = {};
    cr_assert_eq(apl_part_type_service_get(&app, team.id, type.id, &fetched_type, &error), APL_OK);
    cr_assert(fetched_type.meta.archived);

    open_part_unit_dialog(&state, false);
    handle_dialog_input(&state, &input);
    cr_assert_eq(state.dialog_context, SHELL_DIALOG_CONTEXT_PART_UNIT_CREATE);
    cr_assert(apl_tui_dialog_field_at(&state.dialog, 0)->invalid);
    apl_part_unit_filter unit_filter = {.team_id = team.id, .include_archived = true};
    apl_part_unit *listed_units = nullptr;
    size_t listed_count = 0;
    cr_assert_eq(
        apl_part_unit_service_list(&app, &unit_filter, &listed_units, &listed_count, &error),
        APL_OK);
    cr_assert_eq(listed_count, 1);
    apl_part_unit_service_free_list(listed_units);
    input.id = NCKEY_ESC;
    handle_dialog_input(&state, &input);

    apl_part_type_service_free_list(state.part_types);
    apl_part_unit_service_free_list(state.part_units);
    apl_lifecycle_service_free_history(state.unit_events);
    apl_lifecycle_service_free_fitment(state.fitment_rows);
    close_parts_test_app(&app, dir, &error);
}

Test(tui_parts, replacement_can_create_new_unit_inline_before_review)
{
    char dir[APL_TEST_DIR_MAX];
    char path[APL_TEST_PATH_MAX];
    apl_error error;
    apl_error_init(&error);
    apl_app app = {};
    open_parts_test_app(&app, dir, sizeof(dir), path, sizeof(path), &error);

    apl_team team = {};
    cr_assert_eq(apl_team_service_create(&app, "Inline Replacement Team", "", &team, &error),
                 APL_OK);
    apl_car car = {};
    cr_assert_eq(apl_car_service_create(&app, team.id, "SF-26", "", "", &car, &error), APL_OK);
    apl_part_type type = {};
    cr_assert_eq(apl_part_type_service_create(&app, team.id, "Aero", "Wing", "", "", &type, &error),
                 APL_OK);
    apl_part_unit old_unit = {};
    cr_assert_eq(
        apl_part_unit_service_create(&app, team.id, type.id, "OLD", "", "", "", &old_unit, &error),
        APL_OK);
    apl_lifecycle_event event = {};
    cr_assert_eq(apl_lifecycle_service_install(&app, team.id, old_unit.id, car.id, "front",
                                               "2026-01-01", "", "", "", "", &event, &error),
                 APL_OK);

    shell_state state = {};
    state.app = &app;
    state.active_team = team;
    state.active_status = APL_ACTIVE_TEAM_ACTIVE;
    state.section = APL_TUI_SECTION_PARTS;
    state.screen = SHELL_SCREEN_PART_UNIT_DETAIL;
    apl_tui_list_init(&state.list, 8);
    open_part_unit_detail(&state, old_unit);
    open_replace_dialog_direct(&state);
    apl_tui_dialog_set_field_value(&state.dialog, 1, "new");
    apl_tui_dialog_set_field_value(&state.dialog, 2, "SF-26");
    apl_tui_dialog_set_field_value(&state.dialog, 3, "front");
    apl_tui_dialog_set_field_value(&state.dialog, 4, "2026-02-01");

    ncinput input = {.id = NCKEY_ENTER};
    handle_dialog_input(&state, &input);
    cr_assert_eq(state.dialog_context, SHELL_DIALOG_CONTEXT_PART_UNIT_CREATE);
    char type_id[32];
    (void)snprintf(type_id, sizeof(type_id), "%lld", (long long)type.id);
    cr_assert_str_eq(apl_tui_dialog_field_at(&state.dialog, 0)->value, type_id);
    apl_tui_dialog_set_field_value(&state.dialog, 1, "NEW-INLINE");
    handle_dialog_input(&state, &input);
    cr_assert_eq(state.dialog.kind, APL_TUI_DIALOG_CONFIRM);
    cr_assert_eq(state.pending, SHELL_PENDING_APPLY_REPLACE);
    cr_assert_neq(strcmp(state.replace_new_unit, "new"), 0);
    state.dialog.focus = 1;
    handle_dialog_input(&state, &input);

    apl_fitment_row *fitment = nullptr;
    size_t fitment_count = 0;
    cr_assert_eq(
        apl_lifecycle_service_current_fitment(&app, team.id, &fitment, &fitment_count, &error),
        APL_OK);
    cr_assert_eq(fitment_count, 1);
    cr_assert_eq(fitment[0].unit_id, state.part_unit_detail.id);
    cr_assert_neq(fitment[0].unit_id, old_unit.id);
    apl_lifecycle_service_free_fitment(fitment);

    apl_part_unit_service_free_list(state.part_units);
    apl_lifecycle_service_free_fitment(state.fitment_rows);
    apl_lifecycle_service_free_history(state.unit_events);
    close_parts_test_app(&app, dir, &error);
}
