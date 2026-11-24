#include <criterion/criterion.h>

#include "tui/menu.h"

Test(tui_menu, groups_have_explicit_entries_and_headings)
{
    const char *const labels[] = {"Overview", "Settings", "Help", "About"};
    for (size_t index = 0; index < APL_TUI_MENU_GROUP_COUNT; ++index) {
        const apl_tui_menu_group group = (apl_tui_menu_group)index;
        cr_assert_str_eq(apl_tui_menu_group_label(group), labels[index]);
        cr_assert_gt(apl_tui_menu_item_count(group), 0);
        for (size_t item = 0; item < apl_tui_menu_item_count(group); ++item) {
            const apl_tui_menu_item *const entry = apl_tui_menu_item_at(group, item);
            cr_assert_not_null(entry);
            cr_assert_not_null(entry->label);
            if (entry->enabled) {
                cr_assert_neq(entry->action, APL_TUI_MENU_ACTION_NONE);
            } else {
                cr_assert_eq(entry->action, APL_TUI_MENU_ACTION_NONE);
            }
        }
    }
}

Test(tui_menu, headings_share_offsets_with_hit_testing)
{
    for (size_t index = 0; index < APL_TUI_MENU_GROUP_COUNT; ++index) {
        const apl_tui_menu_group group = (apl_tui_menu_group)index;
        const unsigned start = apl_tui_menu_heading_offset(group);
        cr_assert_gt(start, 0);
        cr_assert_eq(apl_tui_menu_group_at_column(start), group);
        cr_assert_eq(apl_tui_menu_group_at_column(start + 1), group);
    }
    cr_assert_eq(apl_tui_menu_group_at_column(0), APL_TUI_MENU_GROUP_COUNT);
    cr_assert_eq(apl_tui_menu_group_at_column(200), APL_TUI_MENU_GROUP_COUNT);
}

Test(tui_menu, opening_keeps_body_state_out_of_the_model)
{
    apl_tui_menu menu;
    apl_tui_menu_init(&menu);

    cr_assert_not(menu.open);
    cr_assert_eq(menu.group_focus, APL_TUI_MENU_OVERVIEW);

    apl_tui_menu_open(&menu, APL_TUI_MENU_SETTINGS);
    cr_assert(menu.open);
    cr_assert_eq(menu.group, APL_TUI_MENU_SETTINGS);
    cr_assert_eq(menu.group_focus, APL_TUI_MENU_SETTINGS);
    cr_assert_eq(menu.item_focus, 0);

    apl_tui_menu_close(&menu);
    cr_assert_not(menu.open);
    cr_assert_eq(menu.group_focus, APL_TUI_MENU_SETTINGS);
}

Test(tui_menu, group_and_item_navigation_wraps)
{
    apl_tui_menu menu;
    apl_tui_menu_init(&menu);

    apl_tui_menu_move_group(&menu, -1);
    cr_assert_eq(menu.group_focus, APL_TUI_MENU_ABOUT);
    apl_tui_menu_move_group(&menu, 1);
    cr_assert_eq(menu.group_focus, APL_TUI_MENU_OVERVIEW);

    apl_tui_menu_open(&menu, APL_TUI_MENU_OVERVIEW);
    const size_t count = apl_tui_menu_item_count(APL_TUI_MENU_OVERVIEW);
    apl_tui_menu_move_item(&menu, -1);
    cr_assert_eq(menu.item_focus, count - 1);
    apl_tui_menu_move_item(&menu, 1);
    cr_assert_eq(menu.item_focus, 0);

    apl_tui_menu_move_group(&menu, 1);
    cr_assert_eq(menu.group, APL_TUI_MENU_SETTINGS);
    cr_assert_eq(menu.item_focus, 0);
}

Test(tui_menu, disabled_entries_cannot_be_activated)
{
    apl_tui_menu menu;
    apl_tui_menu_init(&menu);
    apl_tui_menu_open(&menu, APL_TUI_MENU_OVERVIEW);
    apl_tui_menu_move_item(&menu, 2);
    cr_assert_not_null(apl_tui_menu_item_at(APL_TUI_MENU_OVERVIEW, menu.item_focus));
    cr_assert_not(apl_tui_menu_item_at(APL_TUI_MENU_OVERVIEW, menu.item_focus)->enabled);

    cr_assert_eq(apl_tui_menu_activate(&menu), APL_TUI_MENU_ACTION_NONE);
    cr_assert(menu.open);
}

Test(tui_menu, activation_returns_the_action_and_closes)
{
    apl_tui_menu menu;
    apl_tui_menu_init(&menu);
    apl_tui_menu_open(&menu, APL_TUI_MENU_ABOUT);
    apl_tui_menu_move_item(&menu, 1);

    cr_assert_eq(apl_tui_menu_activate(&menu), APL_TUI_MENU_ACTION_VERSION);
    cr_assert_not(menu.open);
    cr_assert_eq(menu.group_focus, APL_TUI_MENU_ABOUT);
}

Test(tui_menu, popup_rect_is_aligned_and_bounded)
{
    apl_tui_menu menu;
    apl_tui_menu_init(&menu);
    cr_assert_eq(apl_tui_menu_popup_rect(&menu, 24, 80).rows, 0);

    apl_tui_menu_open(&menu, APL_TUI_MENU_HELP);
    const apl_tui_rect rect = apl_tui_menu_popup_rect(&menu, 24, 80);
    cr_assert_eq(rect.y, 1);
    cr_assert_eq(rect.x, apl_tui_menu_heading_offset(APL_TUI_MENU_HELP));
    cr_assert_eq(rect.rows, apl_tui_menu_item_count(APL_TUI_MENU_HELP) + 2);
    cr_assert_leq(rect.x + rect.cols, 80);
    cr_assert_leq(rect.y + rect.rows, 23);

    const apl_tui_rect minimum = apl_tui_menu_popup_rect(&menu, 10, 40);
    cr_assert_gt(minimum.rows, 0);
    cr_assert_leq(minimum.x + minimum.cols, 40);
    cr_assert_leq(minimum.y + minimum.rows, 9);

    cr_assert_eq(apl_tui_menu_popup_rect(&menu, 4, 20).rows, 3);
    cr_assert_eq(apl_tui_menu_popup_rect(&menu, 3, 20).rows, 0);
}
