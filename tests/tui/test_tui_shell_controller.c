#include <string.h>
#include <wchar.h>

#include <criterion/criterion.h>

#include "tui/shell_internal.h"

Test(shell_controller, history_filter_counts_include_all)
{
    cr_assert_eq(history_entity_filter_count(), 8);
    cr_assert_gt(history_action_filter_count(), history_entity_filter_count());
    cr_assert_str_eq(history_entity_filter_name(0), "");
    cr_assert_str_eq(history_entity_filter_name(1), "team");
    cr_assert_str_eq(history_entity_filter_name(7), "budget_plan");
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
    apl_error_set_field(&error, APL_FIELD_NOTES);
    cr_assert_eq(budget_form_error_field(&error), 4);
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
