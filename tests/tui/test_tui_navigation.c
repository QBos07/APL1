#include <criterion/criterion.h>

#include "tui/navigation.h"

Test(tui_navigation, labels_and_default_section)
{
    cr_assert_str_eq(apl_tui_section_label(APL_TUI_SECTION_TEAMS), "Teams");
    cr_assert_str_eq(apl_tui_section_label(APL_TUI_SECTION_EVENTS), "Events & Outcomes");
    cr_assert_str_eq(apl_tui_section_label(APL_TUI_SECTION_COUNT), "Unknown");
    cr_assert_eq(apl_tui_section_default(), APL_TUI_SECTION_TEAMS);
    cr_assert(apl_tui_section_implemented(APL_TUI_SECTION_TEAMS));
    cr_assert_not(apl_tui_section_implemented(APL_TUI_SECTION_OVERVIEW));
}

Test(tui_navigation, next_and_previous_wrap)
{
    cr_assert_eq(apl_tui_section_next(APL_TUI_SECTION_TEAMS), APL_TUI_SECTION_PEOPLE);
    cr_assert_eq(apl_tui_section_previous(APL_TUI_SECTION_TEAMS), APL_TUI_SECTION_OVERVIEW);
    cr_assert_eq(apl_tui_section_next(APL_TUI_SECTION_SETTINGS), APL_TUI_SECTION_OVERVIEW);
    cr_assert_eq(apl_tui_section_previous(APL_TUI_SECTION_OVERVIEW), APL_TUI_SECTION_SETTINGS);
}

Test(tui_navigation, invalid_section_falls_back_to_default)
{
    cr_assert_eq(apl_tui_section_next(APL_TUI_SECTION_COUNT), apl_tui_section_default());
    cr_assert_eq(apl_tui_section_previous((apl_tui_section)999), apl_tui_section_default());
}
