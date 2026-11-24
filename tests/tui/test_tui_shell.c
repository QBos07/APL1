#include <string.h>

#include <criterion/criterion.h>

#include "tui/shell.h"

/*
 * Terminal-free contracts only (see the module header): the shell itself
 * needs a real terminal and is smoke-checked in a pty as recorded in the 1.9
 * work item.
 */

Test(tui_shell, run_requires_a_started_tui)
{
    apl_tui tui = {0};

    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_tui_shell_run(&tui, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_null(tui.notcurses);
    cr_assert_neq(strstr(apl_error_message(&err), "not started"), (char *)NULL);
}

Test(tui_shell, run_accepts_null_tui)
{
    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_tui_shell_run(NULL, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_neq(strstr(apl_error_message(&err), "not started"), (char *)NULL);
}
