#include "tui/theme.h"

#include <notcurses/notcurses.h>

#define RGB(r, g, b) (((uint32_t)(r) << 16U) | ((uint32_t)(g) << 8U) | (uint32_t)(b))

typedef struct role_definition {
    unsigned attrs;
    bool reverse;
    bool has_fg;
    bool has_bg;
    uint32_t fg;
    uint32_t bg;
} role_definition;

static const role_definition ROLES[APL_TUI_ROLE_COUNT] = {
    [APL_TUI_ROLE_WINDOW_BAR] = {NCSTYLE_BOLD, false, true, true, RGB(0xEE, 0xEE, 0xEE),
                                 RGB(0x1F, 0x3A, 0x5F)},
    [APL_TUI_ROLE_WINDOW_FOCUS] = {NCSTYLE_BOLD | NCSTYLE_UNDERLINE, false, true, true,
                                   RGB(0xFF, 0xFF, 0xFF), RGB(0x2D, 0x6C, 0xB0)},
    [APL_TUI_ROLE_MENU] = {0, false, true, true, RGB(0xEE, 0xEE, 0xEE), RGB(0x20, 0x20, 0x20)},
    [APL_TUI_ROLE_MENU_FOCUS] = {NCSTYLE_BOLD | NCSTYLE_UNDERLINE, false, true, true,
                                 RGB(0xFF, 0xFF, 0xFF), RGB(0x2D, 0x6C, 0xB0)},
    [APL_TUI_ROLE_MENU_DISABLED] = {0, false, true, true, RGB(0x80, 0x80, 0x80),
                                    RGB(0x20, 0x20, 0x20)},
    [APL_TUI_ROLE_TAB_ACTIVE] = {NCSTYLE_BOLD, false, true, true, RGB(0xFF, 0xFF, 0xFF),
                                 RGB(0x15, 0x65, 0xC0)},
    [APL_TUI_ROLE_TAB_INACTIVE] = {0, false, true, false, RGB(0xBB, 0xBB, 0xBB), 0},
    [APL_TUI_ROLE_TAB_FOCUS] = {NCSTYLE_BOLD | NCSTYLE_UNDERLINE, false, true, true,
                                RGB(0x00, 0x00, 0x00), RGB(0x7F, 0xD1, 0xFF)},
    [APL_TUI_ROLE_TEAM] = {NCSTYLE_BOLD, false, true, false, RGB(0x2E, 0x7D, 0x32), 0},
    [APL_TUI_ROLE_TEAM_FOCUS] = {NCSTYLE_BOLD | NCSTYLE_UNDERLINE, false, true, true,
                                 RGB(0x00, 0x00, 0x00), RGB(0x9A, 0xE3, 0x7A)},
    [APL_TUI_ROLE_BODY] = {0, false, false, false, 0, 0},
    [APL_TUI_ROLE_SELECTION] = {NCSTYLE_BOLD | NCSTYLE_UNDERLINE, false, true, true,
                                RGB(0xFF, 0xFF, 0xFF), RGB(0x2D, 0x6C, 0xB0)},
    [APL_TUI_ROLE_DIALOG] = {0, false, true, true, RGB(0xF2, 0xF2, 0xF2), RGB(0x28, 0x2C, 0x34)},
    [APL_TUI_ROLE_DIALOG_TITLE] = {NCSTYLE_BOLD | NCSTYLE_UNDERLINE, false, true, true,
                                   RGB(0xFF, 0xFF, 0xFF), RGB(0x28, 0x2C, 0x34)},
    [APL_TUI_ROLE_SUCCESS] = {NCSTYLE_BOLD, false, true, false, RGB(0x7A, 0xD9, 0x7A), 0},
    [APL_TUI_ROLE_WARNING] = {NCSTYLE_BOLD, false, true, false, RGB(0xF2, 0xD0, 0x4C), 0},
    [APL_TUI_ROLE_ERROR] = {NCSTYLE_BOLD, false, true, false, RGB(0xFF, 0x6B, 0x6B), 0},
    [APL_TUI_ROLE_MUTED] = {0, false, true, false, RGB(0x90, 0x90, 0x90), 0},
};

void apl_tui_theme_init(apl_tui_theme *const theme, const bool color_capable)
{
    if (theme == nullptr) {
        return;
    }
    theme->color = color_capable;
}

apl_tui_style apl_tui_theme_style(const apl_tui_theme *const theme, const apl_tui_style_role role)
{
    if (theme == nullptr || role >= APL_TUI_ROLE_COUNT) {
        return (apl_tui_style){};
    }
    const role_definition *const definition = &ROLES[role];
    return (apl_tui_style){
        .attrs = definition->attrs,
        .reverse = definition->reverse,
        .has_fg = theme->color && definition->has_fg,
        .has_bg = theme->color && definition->has_bg,
        .fg = definition->fg,
        .bg = definition->bg,
    };
}

void apl_tui_theme_apply(const apl_tui_theme *const theme, struct ncplane *const plane,
                         const apl_tui_style_role role)
{
    if (plane == nullptr) {
        return;
    }
    const apl_tui_style style = apl_tui_theme_style(theme, role);
    ncplane_set_styles(plane, style.attrs);
    if (style.has_fg) {
        (void)ncplane_set_fg_rgb(plane, style.fg);
    } else {
        ncplane_set_fg_default(plane);
    }
    if (style.has_bg) {
        (void)ncplane_set_bg_rgb(plane, style.bg);
    } else {
        ncplane_set_bg_default(plane);
    }
    if (style.reverse) {
        ncplane_set_channels(plane, ncchannels_reverse(ncplane_channels(plane)));
    }
}

void apl_tui_theme_reset(struct ncplane *const plane)
{
    if (plane == nullptr) {
        return;
    }
    ncplane_set_styles(plane, 0);
    ncplane_set_fg_default(plane);
    ncplane_set_bg_default(plane);
}
