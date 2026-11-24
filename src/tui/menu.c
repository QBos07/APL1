#include "tui/menu.h"

#include <stdint.h>
#include <string.h>

static const char *const GROUP_LABELS[APL_TUI_MENU_GROUP_COUNT] = {
    "Overview", "Records", "Settings", "Help", "About",
};

static const unsigned GROUP_OFFSETS[APL_TUI_MENU_GROUP_COUNT] = {
    1, 11, 20, 30, 36,
};

static const apl_tui_menu_item OVERVIEW_ITEMS[] = {
    {"Active team overview", APL_TUI_MENU_ACTION_OVERVIEW_ACTIVE, true},
    {"All teams overview", APL_TUI_MENU_ACTION_OVERVIEW_ALL, true},
    {"Season overview (not available)", APL_TUI_MENU_ACTION_NONE, false},
};

static const apl_tui_menu_item RECORDS_ITEMS[] = {
    {"People / drivers", APL_TUI_MENU_ACTION_OPEN_PEOPLE, true},
    {"Seasons / periods", APL_TUI_MENU_ACTION_OPEN_SEASONS, true},
    {"Cars / chassis", APL_TUI_MENU_ACTION_OPEN_CARS, true},
};

static const apl_tui_menu_item SETTINGS_ITEMS[] = {
    {"Database information", APL_TUI_MENU_ACTION_DATABASE_INFO, true},
    {"Preferences (not available)", APL_TUI_MENU_ACTION_NONE, false},
};

static const apl_tui_menu_item HELP_ITEMS[] = {
    {"Keyboard and mouse help", APL_TUI_MENU_ACTION_HELP_KEYBOARD, true},
    {"Command help", APL_TUI_MENU_ACTION_HELP_COMMANDS, true},
};

static const apl_tui_menu_item ABOUT_ITEMS[] = {
    {"About apl1", APL_TUI_MENU_ACTION_ABOUT, true},
    {"Version information", APL_TUI_MENU_ACTION_VERSION, true},
};

typedef struct menu_group_definition {
    const apl_tui_menu_item *items;
    size_t count;
} menu_group_definition;

static const menu_group_definition GROUP_ITEMS[APL_TUI_MENU_GROUP_COUNT] = {
    {OVERVIEW_ITEMS, sizeof(OVERVIEW_ITEMS) / sizeof(OVERVIEW_ITEMS[0])},
    {RECORDS_ITEMS, sizeof(RECORDS_ITEMS) / sizeof(RECORDS_ITEMS[0])},
    {SETTINGS_ITEMS, sizeof(SETTINGS_ITEMS) / sizeof(SETTINGS_ITEMS[0])},
    {HELP_ITEMS, sizeof(HELP_ITEMS) / sizeof(HELP_ITEMS[0])},
    {ABOUT_ITEMS, sizeof(ABOUT_ITEMS) / sizeof(ABOUT_ITEMS[0])},
};

static bool valid_group(const apl_tui_menu_group group)
{
    return group >= APL_TUI_MENU_OVERVIEW && group < APL_TUI_MENU_GROUP_COUNT;
}

static size_t group_count(const apl_tui_menu_group group)
{
    return valid_group(group) ? GROUP_ITEMS[group].count : 0;
}

static unsigned min_u(const unsigned left, const unsigned right)
{
    return (left < right) ? left : right;
}

void apl_tui_menu_init(apl_tui_menu *const menu)
{
    if (menu == nullptr) {
        return;
    }
    *menu = (apl_tui_menu){
        .group_focus = APL_TUI_MENU_OVERVIEW,
        .group = APL_TUI_MENU_OVERVIEW,
        .item_focus = 0,
        .open = false,
    };
}

const char *apl_tui_menu_group_label(const apl_tui_menu_group group)
{
    return valid_group(group) ? GROUP_LABELS[group] : "";
}

unsigned apl_tui_menu_heading_offset(const apl_tui_menu_group group)
{
    return valid_group(group) ? GROUP_OFFSETS[group] : 0;
}

apl_tui_menu_group apl_tui_menu_group_at_column(const unsigned column)
{
    for (size_t index = 0; index < APL_TUI_MENU_GROUP_COUNT; ++index) {
        const apl_tui_menu_group group = (apl_tui_menu_group)index;
        const unsigned start = GROUP_OFFSETS[index];
        const unsigned end = start + (unsigned)strlen(GROUP_LABELS[index]);
        if (column >= start && column < end) {
            return group;
        }
    }
    return APL_TUI_MENU_GROUP_COUNT;
}

size_t apl_tui_menu_item_count(const apl_tui_menu_group group)
{
    return group_count(group);
}

const apl_tui_menu_item *apl_tui_menu_item_at(const apl_tui_menu_group group, const size_t index)
{
    if (!valid_group(group) || index >= GROUP_ITEMS[group].count) {
        return nullptr;
    }
    return &GROUP_ITEMS[group].items[index];
}

void apl_tui_menu_focus_group(apl_tui_menu *const menu, const apl_tui_menu_group group)
{
    if (menu == nullptr || !valid_group(group)) {
        return;
    }
    menu->group_focus = group;
    if (menu->open) {
        menu->group = group;
        menu->item_focus = 0;
    }
}

void apl_tui_menu_move_group(apl_tui_menu *const menu, const int delta)
{
    if (menu == nullptr || delta == 0) {
        return;
    }
    int index = (int)menu->group_focus + delta;
    while (index < 0) {
        index += (int)APL_TUI_MENU_GROUP_COUNT;
    }
    index %= (int)APL_TUI_MENU_GROUP_COUNT;
    apl_tui_menu_focus_group(menu, (apl_tui_menu_group)index);
}

void apl_tui_menu_open(apl_tui_menu *const menu, const apl_tui_menu_group group)
{
    if (menu == nullptr || !valid_group(group)) {
        return;
    }
    menu->group_focus = group;
    menu->group = group;
    menu->item_focus = 0;
    menu->open = true;
}

void apl_tui_menu_close(apl_tui_menu *const menu)
{
    if (menu != nullptr) {
        menu->open = false;
    }
}

void apl_tui_menu_move_item(apl_tui_menu *const menu, const int delta)
{
    if (menu == nullptr || !menu->open || delta == 0) {
        return;
    }
    const size_t count = group_count(menu->group);
    if (count == 0) {
        return;
    }
    int index = (int)menu->item_focus + delta;
    while (index < 0) {
        index += (int)count;
    }
    index %= (int)count;
    menu->item_focus = (unsigned)index;
}

apl_tui_menu_action apl_tui_menu_selected_action(const apl_tui_menu *const menu)
{
    if (menu == nullptr || !menu->open) {
        return APL_TUI_MENU_ACTION_NONE;
    }
    const apl_tui_menu_item *const item = apl_tui_menu_item_at(menu->group, menu->item_focus);
    return (item != nullptr && item->enabled) ? item->action : APL_TUI_MENU_ACTION_NONE;
}

apl_tui_menu_action apl_tui_menu_activate(apl_tui_menu *const menu)
{
    const apl_tui_menu_action action = apl_tui_menu_selected_action(menu);
    if (action != APL_TUI_MENU_ACTION_NONE) {
        apl_tui_menu_close(menu);
    }
    return action;
}

apl_tui_rect apl_tui_menu_popup_rect(const apl_tui_menu *const menu, const unsigned terminal_rows,
                                     const unsigned terminal_cols)
{
    if (menu == nullptr || !menu->open || !valid_group(menu->group) || terminal_rows < 3 ||
        terminal_cols < 8) {
        return (apl_tui_rect){};
    }

    const size_t count = group_count(menu->group);
    unsigned width = 28;
    for (size_t index = 0; index < count; ++index) {
        const unsigned label_width = (unsigned)strlen(GROUP_ITEMS[menu->group].items[index].label);
        width = (label_width + 6 > width) ? label_width + 6 : width;
    }
    width = min_u(width, terminal_cols);

    unsigned x = GROUP_OFFSETS[menu->group];
    if (x + width > terminal_cols) {
        x = (terminal_cols > width) ? terminal_cols - width : 0;
    }

    /* Keep the last row free for the status bar. */
    unsigned rows = (unsigned)count + 2;
    const unsigned available_rows = terminal_rows - 1;
    if (rows > available_rows) {
        rows = available_rows;
    }
    if (rows < 3) {
        return (apl_tui_rect){};
    }
    return (apl_tui_rect){.y = 1, .x = x, .rows = rows, .cols = width};
}
