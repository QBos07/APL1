#include <criterion/criterion.h>

#include "tui/record_route.h"

Test(tui_record_route, known_kinds_map_to_their_owning_tab)
{
    cr_assert_eq(apl_tui_record_section(APL_RECORD_TEAM), APL_TUI_SECTION_TEAMS);
    cr_assert_eq(apl_tui_record_section(APL_RECORD_SEASON), APL_TUI_SECTION_TEAMS);
    cr_assert_eq(apl_tui_record_section(APL_RECORD_CAR), APL_TUI_SECTION_TEAMS);
    cr_assert_eq(apl_tui_record_section(APL_RECORD_DRIVER), APL_TUI_SECTION_PEOPLE);
    cr_assert_eq(apl_tui_record_section(APL_RECORD_MEMBERSHIP), APL_TUI_SECTION_PEOPLE);
    cr_assert_eq(apl_tui_record_section(APL_RECORD_LEDGER_ENTRY), APL_TUI_SECTION_MONEY);
    cr_assert_eq(apl_tui_record_section(APL_RECORD_BUDGET_PLAN), APL_TUI_SECTION_MONEY);
    cr_assert_eq(apl_tui_record_section(APL_RECORD_ACTIVITY), APL_TUI_SECTION_HISTORY);
}

Test(tui_record_route, unknown_kinds_fall_back_to_settings)
{
    cr_assert_eq(apl_tui_record_section(APL_RECORD_NONE), APL_TUI_SECTION_HISTORY);
    cr_assert_eq(apl_tui_record_section((apl_record_kind)999), APL_TUI_SECTION_SETTINGS);
}
