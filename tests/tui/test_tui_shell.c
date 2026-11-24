#include <string.h>

#include <criterion/criterion.h>

#include "app/app.h"
#include "tui/shell.h"

/*
 * Terminal-free contracts only (see the module header): the shell itself
 * needs a real terminal and is smoke-checked separately.
 */

Test(tui_shell, run_requires_a_started_tui)
{
    apl_tui tui = {};
    apl_app app = {};

    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_tui_shell_run(&tui, &app, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_null(tui.notcurses);
    cr_assert_neq(strstr(apl_error_message(&err), "started TUI"), nullptr);
}

Test(tui_shell, run_accepts_null_tui_or_app)
{
    apl_app app = {};
    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_tui_shell_run(nullptr, &app, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_neq(strstr(apl_error_message(&err), "started TUI"), nullptr);

    apl_tui tui = {};
    cr_assert_eq(apl_tui_shell_run(&tui, nullptr, &err), APL_ERR_INVALID_ARGUMENT);
}
