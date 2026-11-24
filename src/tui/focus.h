#ifndef APL_TUI_FOCUS_H
#define APL_TUI_FOCUS_H

#include <stdbool.h>

/*
 * Semantic focus regions for the redesigned shell (work items 2.19/2.23).
 *
 * Focus is presentation state only. Modal regions (dialog and command line)
 * remember the previous region so closing them restores the user's context.
 * No business logic depends on this module.
 */

typedef enum apl_tui_focus_region {
    APL_TUI_FOCUS_WINDOW = 0,
    APL_TUI_FOCUS_TABS,
    APL_TUI_FOCUS_BODY,
    APL_TUI_FOCUS_STATUS,
    APL_TUI_FOCUS_COMMAND,
    APL_TUI_FOCUS_DIALOG,
    APL_TUI_FOCUS_REGION_COUNT,
} apl_tui_focus_region;

typedef struct apl_tui_focus {
    apl_tui_focus_region region;
    apl_tui_focus_region return_region;
    bool modal;
} apl_tui_focus;

/** Start with the body focused. */
void apl_tui_focus_init(apl_tui_focus *focus);

/** Set the focused region (invalid/modal regions are ignored). */
void apl_tui_focus_set(apl_tui_focus *focus, apl_tui_focus_region region);

/** Focus a modal region, remembering the current non-modal region.
 *
 * Repeated modal pushes keep the original return region so nested flows cannot
 * lose the user's place. */
void apl_tui_focus_push(apl_tui_focus *focus, apl_tui_focus_region region);

/** Leave a modal region and restore the remembered region. */
void apl_tui_focus_pop(apl_tui_focus *focus);

/** Cycle the non-modal regions in the canonical order
 *  Window -> Tabs -> Body -> Status -> Window. Ignored while modal. */
void apl_tui_focus_cycle(apl_tui_focus *focus, int delta);

/** True while a modal region owns input. */
bool apl_tui_focus_modal(const apl_tui_focus *focus);

#endif /* APL_TUI_FOCUS_H */
