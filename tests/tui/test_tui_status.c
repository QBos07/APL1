#include <string.h>

#include <criterion/criterion.h>

#include "tui/status.h"

Test(tui_status, starts_empty_and_renders_team_placeholder)
{
    apl_tui_status status;
    apl_tui_status_init(&status);

    char line[256];
    const size_t length = apl_tui_status_render(&status, 80, line, sizeof(line));
    cr_assert_eq(length, strlen(line));
    cr_assert_str_eq(line, "(no active team) \xc2\xb7 : commands");
    cr_assert_eq(apl_tui_status_team_hit_width(&status, 80), strlen("(no active team)"));
}

Test(tui_status, shows_team_view_context_message_and_hint)
{
    apl_tui_status status;
    apl_tui_status_init(&status);
    apl_tui_status_set_team(&status, "Ferrari");
    apl_tui_status_set_context(&status, "Teams");
    cr_assert(apl_tui_status_set_message(&status, APL_TUI_STATUS_SUCCESS, "Changes saved."));

    char line[256];
    (void)apl_tui_status_render(&status, 120, line, sizeof(line));
    cr_assert_str_eq(line,
                     "Ferrari \xc2\xb7 Teams \xc2\xb7 [ok] Changes saved. \xc2\xb7 : commands "
                     "\xc2\xb7 m menu \xc2\xb7 t team");
    cr_assert_eq(apl_tui_status_team_hit_width(&status, 120), strlen("Ferrari"));
    cr_assert_neq(apl_tui_status_command_hit_start(&status, 120), 120);

    (void)apl_tui_status_render(&status, 70, line, sizeof(line));
    cr_assert_str_eq(line,
                     "Ferrari \xc2\xb7 Teams \xc2\xb7 [ok] Changes saved. \xc2\xb7 : commands");
}

Test(tui_status, error_is_not_replaced_by_an_unrelated_success)
{
    apl_tui_status status;
    apl_tui_status_init(&status);

    cr_assert(apl_tui_status_set_message(&status, APL_TUI_STATUS_ERROR, "Save failed."));
    cr_assert_not(apl_tui_status_set_message(&status, APL_TUI_STATUS_SUCCESS, "All good."));
    cr_assert_str_eq(status.message, "Save failed.");
    cr_assert_eq(status.level, APL_TUI_STATUS_ERROR);

    cr_assert(apl_tui_status_set_message(&status, APL_TUI_STATUS_ERROR, "Still failed."));
    cr_assert_str_eq(status.message, "Still failed.");

    apl_tui_status_clear_message(&status);
    cr_assert(apl_tui_status_set_message(&status, APL_TUI_STATUS_SUCCESS, "All good."));
    cr_assert_str_eq(status.message, "All good.");
}

Test(tui_status, warning_and_success_replace_each_other_deterministically)
{
    apl_tui_status status;
    apl_tui_status_init(&status);

    cr_assert(apl_tui_status_set_message(&status, APL_TUI_STATUS_WARNING, "Careful."));
    cr_assert(apl_tui_status_set_message(&status, APL_TUI_STATUS_SUCCESS, "Saved."));
    cr_assert_str_eq(status.message, "Saved.");
}

Test(tui_status, narrow_rendering_keeps_team_and_message_and_utf8_intact)
{
    apl_tui_status status;
    apl_tui_status_init(&status);
    apl_tui_status_set_team(&status, "Scuderia Ferrari");
    apl_tui_status_set_context(&status, "Teams");
    (void)apl_tui_status_set_message(&status, APL_TUI_STATUS_WARNING,
                                     "Development notes need a date. \xc3\xa9");

    char line[512];
    (void)apl_tui_status_render(&status, 45, line, sizeof(line));
    cr_assert_neq(strstr(line, "Scuderia Ferrari"), nullptr);
    cr_assert_eq(strstr(line, "Teams"), nullptr);
    cr_assert_neq(strstr(line, "Development notes"), nullptr);
    cr_assert_eq(strstr(line, ": commands"), nullptr);
    cr_assert_leq(strlen(line), 45);
    /* The truncated value must end on a complete UTF-8 sequence. */
    const size_t length = strlen(line);
    cr_assert_neq(length, 0);
    cr_assert_neq((unsigned char)line[length - 1] & 0xC0U, 0x80U);

    char tiny[8];
    (void)apl_tui_status_render(&status, 4, tiny, sizeof(tiny));
    cr_assert_leq(strlen(tiny), 4);
}

Test(tui_status, markers_are_distinct_without_color)
{
    cr_assert_str_eq(apl_tui_status_level_marker(APL_TUI_STATUS_SUCCESS), "[ok] ");
    cr_assert_str_eq(apl_tui_status_level_marker(APL_TUI_STATUS_WARNING), "[!] ");
    cr_assert_str_eq(apl_tui_status_level_marker(APL_TUI_STATUS_ERROR), "[error] ");
    cr_assert_str_eq(apl_tui_status_level_marker(APL_TUI_STATUS_INFO), "[i] ");
    cr_assert_str_eq(apl_tui_status_level_name(APL_TUI_STATUS_ERROR), "error");
}
