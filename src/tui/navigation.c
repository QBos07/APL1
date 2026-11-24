#include "tui/navigation.h"

const char *apl_tui_section_label(const apl_tui_section section)
{
    switch (section) {
    case APL_TUI_SECTION_OVERVIEW:
        return "Overview";
    case APL_TUI_SECTION_TEAMS:
        return "Teams";
    case APL_TUI_SECTION_PEOPLE:
        return "People";
    case APL_TUI_SECTION_EVENTS:
        return "Events & Outcomes";
    case APL_TUI_SECTION_MONEY:
        return "Money";
    case APL_TUI_SECTION_DEVELOPMENT:
        return "Development";
    case APL_TUI_SECTION_PARTS:
        return "Parts";
    case APL_TUI_SECTION_HISTORY:
        return "History";
    case APL_TUI_SECTION_SETTINGS:
        return "Settings";
    case APL_TUI_SECTION_COUNT:
        break;
    }
    return "Unknown";
}

apl_tui_section apl_tui_section_default(void)
{
    return APL_TUI_SECTION_TEAMS;
}

apl_tui_section apl_tui_section_next(const apl_tui_section section)
{
    const int index = (int)section;
    if (index < 0 || index >= (int)APL_TUI_SECTION_COUNT) {
        return apl_tui_section_default();
    }
    return (apl_tui_section)((index + 1) % (int)APL_TUI_SECTION_COUNT);
}

apl_tui_section apl_tui_section_previous(const apl_tui_section section)
{
    const int index = (int)section;
    if (index < 0 || index >= (int)APL_TUI_SECTION_COUNT) {
        return apl_tui_section_default();
    }
    return (apl_tui_section)((index + (int)APL_TUI_SECTION_COUNT - 1) % (int)APL_TUI_SECTION_COUNT);
}

bool apl_tui_section_implemented(const apl_tui_section section)
{
    return section == APL_TUI_SECTION_TEAMS;
}
