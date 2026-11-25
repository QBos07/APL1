#include "tui/record_route.h"

apl_tui_section apl_tui_record_section(const apl_record_kind kind)
{
    switch (kind) {
    case APL_RECORD_TEAM:
        return APL_TUI_SECTION_TEAMS;
    case APL_RECORD_DRIVER:
    case APL_RECORD_MEMBERSHIP:
        return APL_TUI_SECTION_PEOPLE;
    case APL_RECORD_SEASON:
    case APL_RECORD_CAR:
        return APL_TUI_SECTION_TEAMS;
    case APL_RECORD_LEDGER_ENTRY:
    case APL_RECORD_BUDGET_PLAN:
        return APL_TUI_SECTION_MONEY;
    case APL_RECORD_PROJECT:
        return APL_TUI_SECTION_DEVELOPMENT;
    case APL_RECORD_ACTIVITY:
    case APL_RECORD_NONE:
        return APL_TUI_SECTION_HISTORY;
    }
    return APL_TUI_SECTION_SETTINGS;
}
