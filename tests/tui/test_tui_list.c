#include <criterion/criterion.h>

#include "tui/list.h"

Test(tui_list, empty_list_has_no_selection)
{
    apl_tui_list list;
    apl_tui_list_init(&list, 5);

    cr_assert_eq(list.count, 0);
    cr_assert_eq(list.selected, 0);
    cr_assert_eq(list.offset, 0);
    cr_assert_eq(apl_tui_list_visible_count(&list), 0);
    cr_assert_not(apl_tui_list_index_visible(&list, 0));
}

Test(tui_list, movement_clamps_and_keeps_selection_visible)
{
    apl_tui_list list;
    apl_tui_list_init(&list, 3);
    apl_tui_list_set_count(&list, 10);

    apl_tui_list_move(&list, -1);
    cr_assert_eq(list.selected, 0);

    apl_tui_list_move(&list, 5);
    cr_assert_eq(list.selected, 5);
    cr_assert(apl_tui_list_index_visible(&list, 5));
    cr_assert_eq(list.offset, 3);

    apl_tui_list_move(&list, 100);
    cr_assert_eq(list.selected, 9);
    cr_assert_eq(list.offset, 7);
    cr_assert(apl_tui_list_index_visible(&list, 9));

    apl_tui_list_move(&list, -100);
    cr_assert_eq(list.selected, 0);
    cr_assert_eq(list.offset, 0);
}

Test(tui_list, page_home_and_end)
{
    apl_tui_list list;
    apl_tui_list_init(&list, 4);
    apl_tui_list_set_count(&list, 20);

    apl_tui_list_page(&list, 1);
    cr_assert_eq(list.selected, 4);
    apl_tui_list_page(&list, -1);
    cr_assert_eq(list.selected, 0);

    apl_tui_list_end(&list);
    cr_assert_eq(list.selected, 19);
    cr_assert_eq(list.offset, 16);
    apl_tui_list_home(&list);
    cr_assert_eq(list.selected, 0);
    cr_assert_eq(list.offset, 0);
}

Test(tui_list, viewport_resize_preserves_visible_selection)
{
    apl_tui_list list;
    apl_tui_list_init(&list, 10);
    apl_tui_list_set_count(&list, 20);
    apl_tui_list_select(&list, 9);
    cr_assert_eq(list.offset, 0);

    apl_tui_list_set_viewport_rows(&list, 3);
    cr_assert(apl_tui_list_index_visible(&list, 9));
    cr_assert_eq(list.offset, 7);

    apl_tui_list_set_viewport_rows(&list, 0);
    cr_assert_eq(apl_tui_list_visible_count(&list), 0);
    cr_assert_not(apl_tui_list_index_visible(&list, 9));
}

Test(tui_list, shrinking_count_clamps_selection)
{
    apl_tui_list list;
    apl_tui_list_init(&list, 5);
    apl_tui_list_set_count(&list, 10);
    apl_tui_list_select(&list, 9);

    apl_tui_list_set_count(&list, 2);
    cr_assert_eq(list.selected, 1);
    cr_assert_eq(list.offset, 0);

    apl_tui_list_set_count(&list, 0);
    cr_assert_eq(list.selected, 0);
    cr_assert_eq(list.offset, 0);
}

Test(tui_list, visible_count_accounts_for_offset)
{
    apl_tui_list list;
    apl_tui_list_init(&list, 5);
    apl_tui_list_set_count(&list, 8);
    cr_assert_eq(apl_tui_list_visible_count(&list), 5);

    apl_tui_list_select(&list, 7);
    cr_assert_eq(list.offset, 3);
    cr_assert_eq(apl_tui_list_visible_count(&list), 5);
    cr_assert(apl_tui_list_index_visible(&list, 7));
    cr_assert_not(apl_tui_list_index_visible(&list, 2));
}
