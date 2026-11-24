#include <criterion/criterion.h>

#include "tui/theme.h"

Test(tui_theme, roles_are_semantically_distinct_in_color_mode)
{
    apl_tui_theme theme;
    apl_tui_theme_init(&theme, true);

    const apl_tui_style active = apl_tui_theme_style(&theme, APL_TUI_ROLE_TAB_ACTIVE);
    const apl_tui_style inactive = apl_tui_theme_style(&theme, APL_TUI_ROLE_TAB_INACTIVE);
    const apl_tui_style focus = apl_tui_theme_style(&theme, APL_TUI_ROLE_TAB_FOCUS);
    const apl_tui_style success = apl_tui_theme_style(&theme, APL_TUI_ROLE_SUCCESS);
    const apl_tui_style warning = apl_tui_theme_style(&theme, APL_TUI_ROLE_WARNING);
    const apl_tui_style error = apl_tui_theme_style(&theme, APL_TUI_ROLE_ERROR);

    cr_assert_neq(active.attrs, inactive.attrs);
    cr_assert_not(inactive.reverse);
    cr_assert(focus.reverse);
    cr_assert(active.has_fg);
    cr_assert_neq(active.fg, inactive.fg);
    cr_assert_neq(success.fg, warning.fg);
    cr_assert_neq(warning.fg, error.fg);
    cr_assert_neq(success.fg, error.fg);
}

Test(tui_theme, no_color_falls_back_to_attributes_and_markers)
{
    apl_tui_theme theme;
    apl_tui_theme_init(&theme, false);

    for (int role = 0; role < APL_TUI_ROLE_COUNT; ++role) {
        const apl_tui_style style = apl_tui_theme_style(&theme, (apl_tui_style_role)role);
        cr_assert_not(style.has_fg);
        cr_assert_not(style.has_bg);
    }

    const apl_tui_style active = apl_tui_theme_style(&theme, APL_TUI_ROLE_TAB_ACTIVE);
    const apl_tui_style inactive = apl_tui_theme_style(&theme, APL_TUI_ROLE_TAB_INACTIVE);
    const apl_tui_style focus = apl_tui_theme_style(&theme, APL_TUI_ROLE_TAB_FOCUS);
    cr_assert_neq(active.attrs, inactive.attrs);
    cr_assert(focus.reverse);
}

Test(tui_theme, null_inputs_are_safe)
{
    apl_tui_theme theme;
    apl_tui_theme_init(nullptr, true);
    apl_tui_theme_init(&theme, true);

    const apl_tui_style none = apl_tui_theme_style(nullptr, APL_TUI_ROLE_BODY);
    cr_assert_not(none.has_fg);
    apl_tui_theme_apply(&theme, nullptr, APL_TUI_ROLE_ERROR);
    apl_tui_theme_reset(nullptr);
    (void)theme;
}
