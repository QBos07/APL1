#include <stdint.h>

#include <criterion/criterion.h>

#include "tui/navigation.h"

Test(tui_navigation, labels_and_default_section)
{
    cr_assert_str_eq(apl_tui_section_label(APL_TUI_SECTION_TEAMS), "Teams");
    cr_assert_str_eq(apl_tui_section_label(APL_TUI_SECTION_EVENTS), "Events & Outcomes");
    cr_assert_str_eq(apl_tui_section_short_label(APL_TUI_SECTION_EVENTS), "Events");
    cr_assert_str_eq(apl_tui_section_label(APL_TUI_SECTION_COUNT), "Unknown");
    cr_assert_eq(apl_tui_section_default(), APL_TUI_SECTION_TEAMS);
    cr_assert(apl_tui_section_implemented(APL_TUI_SECTION_TEAMS));
    cr_assert(apl_tui_section_implemented(APL_TUI_SECTION_MONEY));
    cr_assert(apl_tui_section_implemented(APL_TUI_SECTION_DEVELOPMENT));
    cr_assert_not(apl_tui_section_implemented(APL_TUI_SECTION_OVERVIEW));
}

Test(tui_navigation, primary_tabs_exclude_settings)
{
    cr_assert_eq(apl_tui_section_tab_count(), 8);
    const apl_tui_section expected[] = {
        APL_TUI_SECTION_OVERVIEW, APL_TUI_SECTION_TEAMS,   APL_TUI_SECTION_PEOPLE,
        APL_TUI_SECTION_EVENTS,   APL_TUI_SECTION_MONEY,   APL_TUI_SECTION_DEVELOPMENT,
        APL_TUI_SECTION_PARTS,    APL_TUI_SECTION_HISTORY,
    };
    for (size_t index = 0; index < apl_tui_section_tab_count(); ++index) {
        cr_assert_eq(apl_tui_section_tab_at(index), expected[index]);
        cr_assert_eq(apl_tui_section_tab_index(expected[index]), index);
        cr_assert_eq(apl_tui_section_tab_index(APL_TUI_SECTION_SETTINGS), SIZE_MAX);
    }
    cr_assert_eq(apl_tui_section_tab_at(apl_tui_section_tab_count()), APL_TUI_SECTION_COUNT);
}

Test(tui_navigation, next_and_previous_wrap_only_primary_tabs)
{
    cr_assert_eq(apl_tui_section_next(APL_TUI_SECTION_TEAMS), APL_TUI_SECTION_PEOPLE);
    cr_assert_eq(apl_tui_section_previous(APL_TUI_SECTION_TEAMS), APL_TUI_SECTION_OVERVIEW);
    cr_assert_eq(apl_tui_section_next(APL_TUI_SECTION_HISTORY), APL_TUI_SECTION_OVERVIEW);
    cr_assert_eq(apl_tui_section_previous(APL_TUI_SECTION_OVERVIEW), APL_TUI_SECTION_HISTORY);
    cr_assert_eq(apl_tui_section_next(APL_TUI_SECTION_SETTINGS), apl_tui_section_default());
    cr_assert_eq(apl_tui_section_previous(APL_TUI_SECTION_SETTINGS), apl_tui_section_default());
}

Test(tui_navigation, invalid_section_falls_back_to_default)
{
    cr_assert_eq(apl_tui_section_next(APL_TUI_SECTION_COUNT), apl_tui_section_default());
    cr_assert_eq(apl_tui_section_previous((apl_tui_section)999), apl_tui_section_default());
}

Test(tui_navigation, names_resolve_case_insensitively)
{
    apl_tui_section section = APL_TUI_SECTION_COUNT;
    cr_assert(apl_tui_section_from_name("money", &section));
    cr_assert_eq(section, APL_TUI_SECTION_MONEY);
    cr_assert(apl_tui_section_from_name("EVENTS & OUTCOMES", &section));
    cr_assert_eq(section, APL_TUI_SECTION_EVENTS);
    cr_assert(apl_tui_section_from_name("outcomes", &section));
    cr_assert_eq(section, APL_TUI_SECTION_EVENTS);
    cr_assert(apl_tui_section_from_name("settings", &section));
    cr_assert_eq(section, APL_TUI_SECTION_SETTINGS);
    cr_assert_not(apl_tui_section_from_name("bogus", &section));
    cr_assert_not(apl_tui_section_from_name(nullptr, &section));
}
