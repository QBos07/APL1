#ifndef APL_TUI_NAVIGATION_H
#define APL_TUI_NAVIGATION_H

#include <stddef.h>

/*
 * Main-view navigation state (work items 2.1 and 2.19).
 *
 * The primary views form the vertical tab rail. Settings is deliberately not a
 * primary tab; it is reached through the window-bar Settings menu. Phase 2
 * implements the Teams view; other views render an honest placeholder until
 * their features exist.
 */

typedef enum apl_tui_section {
    APL_TUI_SECTION_OVERVIEW = 0,
    APL_TUI_SECTION_TEAMS,
    APL_TUI_SECTION_PEOPLE,
    APL_TUI_SECTION_EVENTS,
    APL_TUI_SECTION_MONEY,
    APL_TUI_SECTION_DEVELOPMENT,
    APL_TUI_SECTION_PARTS,
    APL_TUI_SECTION_HISTORY,
    APL_TUI_SECTION_SETTINGS,
    APL_TUI_SECTION_COUNT,
} apl_tui_section;

/** Full label used in the body heading and status context. */
const char *apl_tui_section_label(apl_tui_section section);

/** Shorter label used when the tab rail is compact. */
const char *apl_tui_section_short_label(apl_tui_section section);

/** Section shown when the application starts. Teams is the first implemented
 *  workspace in Phase 2. */
apl_tui_section apl_tui_section_default(void);

/** Number of primary views in the vertical tab rail. */
size_t apl_tui_section_tab_count(void);

/** Primary view at a tab index, or COUNT when out of range. */
apl_tui_section apl_tui_section_tab_at(size_t index);

/** Index of a primary view in the tab rail, or SIZE_MAX for Settings/unknown. */
size_t apl_tui_section_tab_index(apl_tui_section section);

/** Next primary view with wrap-around. */
apl_tui_section apl_tui_section_next(apl_tui_section section);

/** Previous primary view with wrap-around. */
apl_tui_section apl_tui_section_previous(apl_tui_section section);

/** Resolve a user-entered view name (including common aliases). */
bool apl_tui_section_from_name(const char *name, apl_tui_section *out_section);

/** True when this section has real screens in the current phase. */
bool apl_tui_section_implemented(apl_tui_section section);

#endif /* APL_TUI_NAVIGATION_H */
