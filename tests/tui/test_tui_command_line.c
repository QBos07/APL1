#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "tui/command_line.h"

Test(tui_command_line, opens_empty_and_cancel_discards_text)
{
    apl_tui_command_line line;
    apl_tui_command_line_init(&line);
    cr_assert_not(apl_tui_command_line_is_open(&line));

    apl_tui_command_line_open(&line);
    cr_assert(apl_tui_command_line_is_open(&line));
    apl_tui_command_line_insert(&line, "view teams");
    cr_assert_str_eq(line.text, "view teams");

    apl_tui_command_line_close(&line);
    cr_assert_not(apl_tui_command_line_is_open(&line));
    cr_assert_str_empty(line.text);
}

Test(tui_command_line, colon_is_ordinary_text_and_backspace_handles_utf8)
{
    apl_tui_command_line line;
    apl_tui_command_line_init(&line);
    apl_tui_command_line_open(&line);

    apl_tui_command_line_insert(&line, ":literal");
    cr_assert_str_eq(line.text, ":literal");
    (void)apl_tui_command_line_backspace(&line);
    cr_assert_str_eq(line.text, ":litera");

    apl_tui_command_line_insert(&line, " \xc3\xa9");
    (void)apl_tui_command_line_backspace(&line);
    cr_assert_str_eq(line.text, ":litera ");
}

Test(tui_command_line, completion_can_replace_the_prompt_text)
{
    apl_tui_command_line line;
    apl_tui_command_line_init(&line);
    apl_tui_command_line_open(&line);
    apl_tui_command_line_insert(&line, "view t");
    apl_tui_command_line_set_suggestion(&line, "view teams");
    cr_assert_str_eq(line.suggestion, "view teams");

    apl_tui_command_line_set_text(&line, line.suggestion);
    cr_assert_str_eq(line.text, "view teams");
}

Test(tui_command_line, history_is_session_local_and_wraps_to_empty)
{
    apl_tui_command_line line;
    apl_tui_command_line_init(&line);
    apl_tui_command_line_open(&line);
    apl_tui_command_line_add_history(&line, "view teams");
    apl_tui_command_line_add_history(&line, "team switch Ferrari");
    cr_assert_eq(line.history_count, 2);

    apl_tui_command_line_history_previous(&line);
    cr_assert_str_eq(line.text, "team switch Ferrari");
    apl_tui_command_line_history_previous(&line);
    cr_assert_str_eq(line.text, "view teams");
    apl_tui_command_line_history_previous(&line);
    cr_assert_str_eq(line.text, "view teams");

    apl_tui_command_line_history_next(&line);
    cr_assert_str_eq(line.text, "team switch Ferrari");
    apl_tui_command_line_history_next(&line);
    cr_assert_str_empty(line.text);
}

Test(tui_command_line, history_is_bounded)
{
    apl_tui_command_line line;
    apl_tui_command_line_init(&line);
    char command[32];
    for (unsigned index = 0; index < APL_TUI_COMMAND_LINE_HISTORY_MAX + 5; ++index) {
        (void)snprintf(command, sizeof(command), "view %u", index);
        apl_tui_command_line_add_history(&line, command);
    }
    cr_assert_eq(line.history_count, APL_TUI_COMMAND_LINE_HISTORY_MAX);
    cr_assert_str_eq(line.history[0], "view 5");
    cr_assert_str_eq(line.history[APL_TUI_COMMAND_LINE_HISTORY_MAX - 1], "view 24");
}

Test(tui_command_line, renders_prompt_and_completion_within_width)
{
    apl_tui_command_line line;
    apl_tui_command_line_init(&line);
    apl_tui_command_line_open(&line);
    apl_tui_command_line_insert(&line, "team c");
    apl_tui_command_line_set_suggestion(&line, "team create [--name <name>]");

    char rendered[256];
    (void)apl_tui_command_line_render(&line, 80, rendered, sizeof(rendered));
    cr_assert_str_eq(rendered, ":team c   team create [--name <name>]");

    (void)apl_tui_command_line_render(&line, 10, rendered, sizeof(rendered));
    cr_assert_leq(strlen(rendered), 10);
    cr_assert_str_eq(rendered, ":team c");

    const size_t length = strlen(rendered);
    cr_assert_neq((unsigned char)rendered[length - 1] & 0xC0U, 0x80U);
}
