#ifndef APL_TUI_MENU_H
#define APL_TUI_MENU_H

#include <stdbool.h>
#include <stddef.h>

#include "tui/layout.h"

/*
 * Window-bar dropdown model (work item 2.17).
 *
 * Pure state: menu groups, their entries, focus, and activation are independent
 * of Notcurses and persistence. Opening a menu never changes the active team,
 * the body view, or any data. Rendering and hit handling live in the shell.
 */

typedef enum apl_tui_menu_group {
    APL_TUI_MENU_OVERVIEW = 0,
    APL_TUI_MENU_SETTINGS,
    APL_TUI_MENU_HELP,
    APL_TUI_MENU_ABOUT,
    APL_TUI_MENU_GROUP_COUNT,
} apl_tui_menu_group;

typedef enum apl_tui_menu_action {
    APL_TUI_MENU_ACTION_NONE = 0,
    APL_TUI_MENU_ACTION_OVERVIEW_ACTIVE,
    APL_TUI_MENU_ACTION_OVERVIEW_ALL,
    APL_TUI_MENU_ACTION_DATABASE_INFO,
    APL_TUI_MENU_ACTION_HELP_KEYBOARD,
    APL_TUI_MENU_ACTION_HELP_COMMANDS,
    APL_TUI_MENU_ACTION_ABOUT,
    APL_TUI_MENU_ACTION_VERSION,
} apl_tui_menu_action;

typedef struct apl_tui_menu_item {
    const char *label;
    apl_tui_menu_action action;
    bool enabled;
} apl_tui_menu_item;

typedef struct apl_tui_menu {
    apl_tui_menu_group group_focus;
    apl_tui_menu_group group;
    unsigned item_focus;
    bool open;
} apl_tui_menu;

/** Reset to closed with Overview focused. */
void apl_tui_menu_init(apl_tui_menu *menu);

/** Group heading shown in the window bar. */
const char *apl_tui_menu_group_label(apl_tui_menu_group group);

/** Absolute column where a group heading starts (shared by drawing and mouse
 *  hit testing). Returns 0 for invalid groups. */
unsigned apl_tui_menu_heading_offset(apl_tui_menu_group group);

/** Group whose heading contains `column`, or GROUP_COUNT for no heading. */
apl_tui_menu_group apl_tui_menu_group_at_column(unsigned column);

/** Number of entries in a group. */
size_t apl_tui_menu_item_count(apl_tui_menu_group group);

/** Entry at `index`, or nullptr when the group/index is invalid. */
const apl_tui_menu_item *apl_tui_menu_item_at(apl_tui_menu_group group, size_t index);

/** Focus a group in the window bar without opening it. */
void apl_tui_menu_focus_group(apl_tui_menu *menu, apl_tui_menu_group group);

/** Move the group focus, wrapping. When a dropdown is open the open group
 *  follows the focus and item focus resets. */
void apl_tui_menu_move_group(apl_tui_menu *menu, int delta);

/** Open the dropdown for `group`, focusing its first entry. */
void apl_tui_menu_open(apl_tui_menu *menu, apl_tui_menu_group group);

/** Close the dropdown, keeping the group focus. */
void apl_tui_menu_close(apl_tui_menu *menu);

/** Move entry focus with wrap. No-op while closed. */
void apl_tui_menu_move_item(apl_tui_menu *menu, int delta);

/** Activate the focused entry.
 *
 * Returns the selected action and closes the menu when the entry is enabled.
 * Disabled/placeholder entries return ACTION_NONE and leave the menu open. */
apl_tui_menu_action apl_tui_menu_activate(apl_tui_menu *menu);

/** Action of the focused entry without activating it (NONE when disabled). */
apl_tui_menu_action apl_tui_menu_selected_action(const apl_tui_menu *menu);

/** Dropdown rectangle aligned under the open group's heading.
 *
 * `terminal_rows`/`terminal_cols` bound the result; the popup never covers the
 * status row and never leaves the terminal. Returns a zero row count when the
 * menu is closed or the terminal is too small. */
apl_tui_rect apl_tui_menu_popup_rect(const apl_tui_menu *menu, unsigned terminal_rows,
                                     unsigned terminal_cols);

#endif /* APL_TUI_MENU_H */
