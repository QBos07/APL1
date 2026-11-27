#include <string.h>
#include <wchar.h>

#include <criterion/criterion.h>

#include "tui/shell_internal.h"

Test(shell_controller, history_filter_counts_include_all)
{
    cr_assert_eq(history_entity_filter_count(), 10);
    cr_assert_gt(history_action_filter_count(), history_entity_filter_count());
    cr_assert_str_eq(history_entity_filter_name(0), "");
    cr_assert_str_eq(history_entity_filter_name(1), "team");
    cr_assert_str_eq(history_entity_filter_name(7), "budget_plan");
    cr_assert_str_eq(history_entity_filter_name(8), "project");
    cr_assert_str_eq(history_entity_filter_name(9), "project_update");
    cr_assert_str_eq(history_action_filter_name(0), "");
    cr_assert_str_eq(history_action_filter_name(1), "team_created");
}

Test(shell_controller, money_and_budget_filter_bars_are_stable)
{
    shell_state state = {};
    state.money_date_preset = 1;
    state.money_kind_filter = 2;
    (void)snprintf(state.money_category_filter, sizeof(state.money_category_filter), "tyres");
    state.budget_date_preset = 3;
    (void)snprintf(state.budget_category_filter, sizeof(state.budget_category_filter), "fuel");

    char out[320];
    unsigned starts[4] = {};
    unsigned ends[4] = {};
    build_money_filter_bar(&state, out, sizeof(out), starts, ends);
    cr_assert_str_eq(out, "Date:30d  Kind:income  Category:tyres  Clear  ");
    cr_assert_lt(starts[0], ends[0]);
    cr_assert_lt(ends[0], starts[1]);
    cr_assert_lt(ends[1], starts[2]);
    cr_assert_lt(ends[2], starts[3]);
    cr_assert_eq(starts[3], (unsigned)strlen("Date:30d  Kind:income  Category:tyres  "));

    build_budget_filter_bar(&state, out, sizeof(out), starts, ends);
    cr_assert_str_eq(out, "Date:365d  Category:fuel  Clear  ");
    cr_assert_lt(ends[0], starts[1]);
    cr_assert_eq(starts[1], (unsigned)strlen("Date:365d  "));
}

Test(shell_controller, list_navigation_is_shared_without_a_terminal)
{
    shell_state state = {};
    apl_tui_list_init(&state.list, 5);
    apl_tui_list_set_count(&state.list, 10);
    apl_tui_list_select(&state.list, 5);

    cr_assert(handle_body_list_navigation(&state, NCKEY_UP));
    cr_assert_eq(state.list.selected, 4);
    cr_assert(handle_body_list_navigation(&state, NCKEY_DOWN));
    cr_assert_eq(state.list.selected, 5);
    cr_assert(handle_body_list_navigation(&state, NCKEY_HOME));
    cr_assert_eq(state.list.selected, 0);
    cr_assert(handle_body_list_navigation(&state, NCKEY_END));
    cr_assert_eq(state.list.selected, 9);
    cr_assert_not(handle_body_list_navigation(&state, 'x'));
}

Test(shell_controller, text_helpers_fit_rows_and_validate_numbers)
{
    char text[32];
    fit_text_with_ellipsis(text, sizeof(text), "short", 10);
    cr_assert_str_eq(text, "short");
    fit_text_with_ellipsis(text, sizeof(text), "long activity summary", 10);
    cr_assert_str_eq(text, "long ac...");
    fit_text_with_ellipsis(text, sizeof(text), "abcdef", 3);
    cr_assert_str_eq(text, "abc");

    cr_assert(text_is_numeric("123"));
    cr_assert_not(text_is_numeric(""));
    cr_assert_not(text_is_numeric("12a"));
}

Test(shell_controller, form_error_fields_map_to_dialog_inputs)
{
    apl_error error;
    apl_error_init(&error);
    apl_error_set(&error, APL_ERR_INVALID_ARGUMENT, "bad amount");
    apl_error_set_field(&error, APL_FIELD_AMOUNT);
    cr_assert_eq(money_form_error_field(&error), 0);
    apl_error_set_field(&error, APL_FIELD_DATE);
    cr_assert_eq(money_form_error_field(&error), 1);
    apl_error_set_field(&error, APL_FIELD_DESCRIPTION);
    cr_assert_eq(money_form_error_field(&error), 3);
    apl_error_set_field(&error, APL_FIELD_PERIOD);
    cr_assert_eq(budget_form_error_field(&error), 0);
    apl_error_set_field(&error, APL_FIELD_PROJECT);
    cr_assert_eq(budget_form_error_field(&error), 4);
    apl_error_set_field(&error, APL_FIELD_NOTES);
    cr_assert_eq(budget_form_error_field(&error), 5);
}

Test(shell_controller, each_tab_keeps_its_own_list_position)
{
    shell_state state = {};
    apl_tui_list_init(&state.list, 5);
    apl_tui_list_set_count(&state.list, 10);
    apl_tui_list_select(&state.list, 7);
    state.section = APL_TUI_SECTION_HISTORY;
    shell_save_view_state(&state, APL_TUI_SECTION_HISTORY);

    apl_tui_list_init(&state.list, 5);
    apl_tui_list_set_count(&state.list, 4);
    apl_tui_list_select(&state.list, 1);
    shell_save_view_state(&state, APL_TUI_SECTION_MONEY);

    shell_restore_view_state(&state, APL_TUI_SECTION_HISTORY);
    cr_assert_eq(state.list.selected, 7);
    cr_assert_eq(state.list.count, 10);
    shell_restore_view_state(&state, APL_TUI_SECTION_MONEY);
    cr_assert_eq(state.list.selected, 1);
    cr_assert_eq(state.list.count, 4);

    /* A tab that has not been visited starts from a clean list. */
    shell_restore_view_state(&state, APL_TUI_SECTION_PEOPLE);
    cr_assert_eq(state.list.selected, 0);
    cr_assert_eq(state.list.count, 0);
}

Test(shell_controller, project_filter_bar_and_form_error_fields_are_stable)
{
    shell_state state = {};
    (void)snprintf(state.project_status_filter, sizeof(state.project_status_filter), "in progress");
    (void)snprintf(state.project_category_filter, sizeof(state.project_category_filter), "aero");
    state.include_archived_projects = true;

    char out[512];
    unsigned starts[5] = {};
    unsigned ends[5] = {};
    build_project_filter_bar(&state, out, sizeof(out), starts, ends);
    cr_assert_str_eq(out, "Status:in progress  Season:any  Category:aero  Archived:yes  Clear  ");
    for (unsigned index = 0; index + 1 < 5; ++index) {
        cr_assert_lt(ends[index], starts[index + 1]);
    }

    apl_error error;
    apl_error_init(&error);
    apl_error_set_field(&error, APL_FIELD_TITLE);
    cr_assert_eq(project_form_error_field(&error), 0);
    apl_error_set_field(&error, APL_FIELD_DESCRIPTION);
    cr_assert_eq(project_form_error_field(&error), 2);
    apl_error_set_field(&error, APL_FIELD_TARGET_DATE);
    cr_assert_eq(project_form_error_field(&error), 4);
    apl_error_set_field(&error, APL_FIELD_CAR);
    cr_assert_eq(project_form_error_field(&error), 7);
    apl_error_set_field(&error, APL_FIELD_SUMMARY);
    cr_assert_eq(project_update_form_error_field(&error), 1);
    apl_error_set_field(&error, APL_FIELD_UPDATE_DATE);
    cr_assert_eq(project_update_form_error_field(&error), 0);
}

Test(shell_controller, parts_filter_bar_and_form_error_fields_are_stable)
{
    shell_state state = {};
    (void)snprintf(state.parts_action_filter, sizeof(state.parts_action_filter), "replace");
    (void)snprintf(state.parts_from_date, sizeof(state.parts_from_date), "2026-01-01");
    state.include_archived_part_units = true;

    char out[512];
    unsigned starts[6] = {};
    unsigned ends[6] = {};
    build_parts_filter_bar(&state, out, sizeof(out), starts, ends);
    cr_assert_str_eq(out, "Action:replace  Car:any  Position:any  Dates:2026-01-01..any  "
                          "Search:any  Type:any  ");
    for (unsigned index = 0; index + 1 < 6; ++index) {
        cr_assert_lt(ends[index], starts[index + 1]);
    }

    apl_error error;
    apl_error_init(&error);
    apl_error_set_field(&error, APL_FIELD_CATEGORY);
    cr_assert_eq(part_type_form_error_field(&error), 0);
    apl_error_set_field(&error, APL_FIELD_NAME);
    cr_assert_eq(part_type_form_error_field(&error), 1);
    apl_error_set_field(&error, APL_FIELD_DESCRIPTION);
    cr_assert_eq(part_type_form_error_field(&error), 2);

    apl_error_clear(&error);
    apl_error_set_field(&error, APL_FIELD_SERIAL_NUMBER);
    cr_assert_eq(part_unit_form_error_field(&error), 1);
    apl_error_set_field(&error, APL_FIELD_CONDITION);
    cr_assert_eq(part_unit_form_error_field(&error), 3);

    apl_error_init(&error);
    apl_error_set_field(&error, APL_FIELD_DATE);
    cr_assert_eq(lifecycle_form_error_field(&error), 3);
    apl_error_set_field(&error, APL_FIELD_POSITION);
    cr_assert_eq(lifecycle_form_error_field(&error), 2);
    apl_error_set_field(&error, APL_FIELD_CAR);
    cr_assert_eq(lifecycle_form_error_field(&error), 1);
}

Test(shell_controller, lifecycle_action_codes_and_labels_map_stably)
{
    cr_assert_eq(apl_lifecycle_action_from_code("install"), APL_LIFECYCLE_INSTALL);
    cr_assert_eq(apl_lifecycle_action_from_code("replace"), APL_LIFECYCLE_REPLACE);
    cr_assert_eq(apl_lifecycle_action_from_code("dispose"), APL_LIFECYCLE_DISPOSE);
    cr_assert_str_eq(apl_lifecycle_action_label(APL_LIFECYCLE_REPAIR), "Repair");
    cr_assert_str_eq(apl_lifecycle_action_label(APL_LIFECYCLE_REMOVE), "Remove");
    cr_assert_str_eq(apl_lifecycle_action_label(APL_LIFECYCLE_NONE), "Unknown");
}
