#ifndef APL_TUI_NAVIGATION_H
#define APL_TUI_NAVIGATION_H

/*
 * Main-section navigation state (work item 2.1).
 *
 * The section list and movement rules are independent of drawing code. Phase 2
 * implements the Teams section; other sections render an honest placeholder
 * until their features exist.
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

/** Short label used in the header. */
const char *apl_tui_section_label(apl_tui_section section);

/** Section shown when the application starts. Teams is the first implemented
 *  workspace in Phase 2. */
apl_tui_section apl_tui_section_default(void);

/** Next section with wrap-around. */
apl_tui_section apl_tui_section_next(apl_tui_section section);

/** Previous section with wrap-around. */
apl_tui_section apl_tui_section_previous(apl_tui_section section);

/** True when this section has real screens in the current phase. */
bool apl_tui_section_implemented(apl_tui_section section);

#endif /* APL_TUI_NAVIGATION_H */
