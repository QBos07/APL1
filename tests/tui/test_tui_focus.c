#include <criterion/criterion.h>

#include "tui/focus.h"

Test(tui_focus, starts_in_the_body_and_cycles_in_order)
{
    apl_tui_focus focus;
    apl_tui_focus_init(&focus);
    cr_assert_eq(focus.region, APL_TUI_FOCUS_BODY);

    apl_tui_focus_cycle(&focus, 1);
    cr_assert_eq(focus.region, APL_TUI_FOCUS_STATUS);
    apl_tui_focus_cycle(&focus, 1);
    cr_assert_eq(focus.region, APL_TUI_FOCUS_WINDOW);
    apl_tui_focus_cycle(&focus, 1);
    cr_assert_eq(focus.region, APL_TUI_FOCUS_TABS);
    apl_tui_focus_cycle(&focus, 1);
    cr_assert_eq(focus.region, APL_TUI_FOCUS_BODY);

    apl_tui_focus_cycle(&focus, -1);
    cr_assert_eq(focus.region, APL_TUI_FOCUS_TABS);
    apl_tui_focus_cycle(&focus, -1);
    cr_assert_eq(focus.region, APL_TUI_FOCUS_WINDOW);
}

Test(tui_focus, modal_focus_remembers_and_restores_the_region)
{
    apl_tui_focus focus;
    apl_tui_focus_init(&focus);
    apl_tui_focus_set(&focus, APL_TUI_FOCUS_TABS);

    apl_tui_focus_push(&focus, APL_TUI_FOCUS_DIALOG);
    cr_assert(apl_tui_focus_modal(&focus));
    cr_assert_eq(focus.region, APL_TUI_FOCUS_DIALOG);
    cr_assert_eq(focus.return_region, APL_TUI_FOCUS_TABS);

    /* A nested modal keeps the original return region. */
    apl_tui_focus_push(&focus, APL_TUI_FOCUS_COMMAND);
    cr_assert_eq(focus.return_region, APL_TUI_FOCUS_TABS);

    apl_tui_focus_pop(&focus);
    cr_assert_not(apl_tui_focus_modal(&focus));
    cr_assert_eq(focus.region, APL_TUI_FOCUS_TABS);

    /* Cycling is ignored while modal. */
    apl_tui_focus_push(&focus, APL_TUI_FOCUS_DIALOG);
    apl_tui_focus_cycle(&focus, 1);
    cr_assert_eq(focus.region, APL_TUI_FOCUS_DIALOG);
}

Test(tui_focus, invalid_regions_are_ignored)
{
    apl_tui_focus focus;
    apl_tui_focus_init(&focus);
    apl_tui_focus_set(&focus, APL_TUI_FOCUS_DIALOG);
    cr_assert_eq(focus.region, APL_TUI_FOCUS_BODY);

    apl_tui_focus_push(&focus, APL_TUI_FOCUS_BODY);
    cr_assert_not(apl_tui_focus_modal(&focus));
}
