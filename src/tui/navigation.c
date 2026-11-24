#include "tui/navigation.h"

#include <stdint.h>

#include "tui/text.h"

typedef struct section_definition {
    apl_tui_section section;
    const char *label;
    const char *short_label;
    const char *aliases[4];
} section_definition;

static const section_definition SECTIONS[] = {
    {APL_TUI_SECTION_OVERVIEW, "Overview", "Overview", {"overview", "home", nullptr, nullptr}},
    {APL_TUI_SECTION_TEAMS, "Teams", "Teams", {"teams", "team", nullptr, nullptr}},
    {APL_TUI_SECTION_PEOPLE, "People", "People", {"people", "drivers", nullptr, nullptr}},
    {APL_TUI_SECTION_EVENTS, "Events & Outcomes", "Events",
     {"events", "events & outcomes", "outcomes", nullptr}},
    {APL_TUI_SECTION_MONEY, "Money", "Money", {"money", "finance", nullptr, nullptr}},
    {APL_TUI_SECTION_DEVELOPMENT, "Development", "Develop",
     {"development", "dev", nullptr, nullptr}},
    {APL_TUI_SECTION_PARTS, "Parts", "Parts", {"parts", "part", nullptr, nullptr}},
    {APL_TUI_SECTION_HISTORY, "History", "History", {"history", "activity", nullptr, nullptr}},
};

constexpr size_t TAB_COUNT = sizeof(SECTIONS) / sizeof(SECTIONS[0]);

static const section_definition *find_definition(const apl_tui_section section)
{
    for (size_t index = 0; index < TAB_COUNT; ++index) {
        if (SECTIONS[index].section == section) {
            return &SECTIONS[index];
        }
    }
    return nullptr;
}

const char *apl_tui_section_label(const apl_tui_section section)
{
    if (section == APL_TUI_SECTION_SETTINGS) {
        return "Settings";
    }
    const section_definition *const definition = find_definition(section);
    return (definition != nullptr) ? definition->label : "Unknown";
}

const char *apl_tui_section_short_label(const apl_tui_section section)
{
    if (section == APL_TUI_SECTION_SETTINGS) {
        return "Settings";
    }
    const section_definition *const definition = find_definition(section);
    return (definition != nullptr) ? definition->short_label : "Unknown";
}

apl_tui_section apl_tui_section_default(void)
{
    return APL_TUI_SECTION_TEAMS;
}

size_t apl_tui_section_tab_count(void)
{
    return TAB_COUNT;
}

apl_tui_section apl_tui_section_tab_at(const size_t index)
{
    return (index < TAB_COUNT) ? SECTIONS[index].section : APL_TUI_SECTION_COUNT;
}

size_t apl_tui_section_tab_index(const apl_tui_section section)
{
    for (size_t index = 0; index < TAB_COUNT; ++index) {
        if (SECTIONS[index].section == section) {
            return index;
        }
    }
    return SIZE_MAX;
}

apl_tui_section apl_tui_section_next(const apl_tui_section section)
{
    const size_t index = apl_tui_section_tab_index(section);
    if (index == SIZE_MAX) {
        return apl_tui_section_default();
    }
    return SECTIONS[(index + 1) % TAB_COUNT].section;
}

apl_tui_section apl_tui_section_previous(const apl_tui_section section)
{
    const size_t index = apl_tui_section_tab_index(section);
    if (index == SIZE_MAX) {
        return apl_tui_section_default();
    }
    return SECTIONS[(index + TAB_COUNT - 1) % TAB_COUNT].section;
}

bool apl_tui_section_from_name(const char *const name, apl_tui_section *const out_section)
{
    if (name == nullptr || out_section == nullptr) {
        return false;
    }
    for (size_t index = 0; index < TAB_COUNT; ++index) {
        for (size_t alias = 0; alias < 4; ++alias) {
            const char *const candidate = SECTIONS[index].aliases[alias];
            if (candidate != nullptr && apl_tui_text_equal_ci(name, candidate)) {
                *out_section = SECTIONS[index].section;
                return true;
            }
        }
    }
    if (apl_tui_text_equal_ci(name, "settings")) {
        *out_section = APL_TUI_SECTION_SETTINGS;
        return true;
    }
    return false;
}

bool apl_tui_section_implemented(const apl_tui_section section)
{
    return section == APL_TUI_SECTION_TEAMS;
}
