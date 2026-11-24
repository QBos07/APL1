#ifndef APL_TUI_THEME_H
#define APL_TUI_THEME_H

#include <stdbool.h>
#include <stdint.h>

struct ncplane;

/*
 * Central semantic style roles for the TUI (work item 2.22).
 *
 * Screens ask for a role (window bar, focused tab, error, ...) instead of
 * inventing color pairs. In color-capable terminals the roles use one coherent
 * palette; otherwise they fall back to attributes and reverse video. Textual
 * markers in labels/status remain the primary signal, so hue is never the only
 * distinction.
 */

typedef enum apl_tui_style_role {
    APL_TUI_ROLE_WINDOW_BAR = 0,
    APL_TUI_ROLE_WINDOW_FOCUS,
    APL_TUI_ROLE_MENU,
    APL_TUI_ROLE_MENU_FOCUS,
    APL_TUI_ROLE_MENU_DISABLED,
    APL_TUI_ROLE_TAB_ACTIVE,
    APL_TUI_ROLE_TAB_INACTIVE,
    APL_TUI_ROLE_TAB_FOCUS,
    APL_TUI_ROLE_TEAM,
    APL_TUI_ROLE_TEAM_FOCUS,
    APL_TUI_ROLE_BODY,
    APL_TUI_ROLE_SELECTION,
    APL_TUI_ROLE_DIALOG,
    APL_TUI_ROLE_DIALOG_TITLE,
    APL_TUI_ROLE_SUCCESS,
    APL_TUI_ROLE_WARNING,
    APL_TUI_ROLE_ERROR,
    APL_TUI_ROLE_MUTED,
    APL_TUI_ROLE_COUNT,
} apl_tui_style_role;

typedef struct apl_tui_style {
    unsigned attrs;
    bool reverse;
    bool has_fg;
    bool has_bg;
    uint32_t fg;
    uint32_t bg;
} apl_tui_style;

typedef struct apl_tui_theme {
    bool color;
} apl_tui_theme;

/** Initialize from the terminal's color capability. */
void apl_tui_theme_init(apl_tui_theme *theme, bool color_capable);

/** Resolve a role for the current capability. */
apl_tui_style apl_tui_theme_style(const apl_tui_theme *theme, apl_tui_style_role role);

/** Apply a role to a plane. Null planes are ignored. */
void apl_tui_theme_apply(const apl_tui_theme *theme, struct ncplane *plane,
                          apl_tui_style_role role);

/** Restore default styles/colors after drawing a styled run. */
void apl_tui_theme_reset(struct ncplane *plane);

#endif /* APL_TUI_THEME_H */
