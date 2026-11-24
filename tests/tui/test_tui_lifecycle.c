#include <string.h>

#include <criterion/criterion.h>

#include "tui/lifecycle.h"

/*
 * These tests exercise only lifecycle contracts that do not require a real
 * terminal (no notcurses_init call). Startup/shutdown inside a supported
 * interactive terminal is verified by the manual smoke check recorded in the
 * 1.8 work item.
 */

Test(tui_lifecycle, stop_on_uninitialized_context_is_safe)
{
    apl_tui tui = {0};
    apl_tui_stop(&tui); /* must not crash or call notcurses_stop */
    apl_tui_stop(&tui); /* repeated stop is a documented no-op */

    cr_assert_null(tui.notcurses);
}

Test(tui_lifecycle, stop_accepts_null_context)
{
    apl_tui_stop(NULL);
    cr_assert(true);
}

Test(tui_lifecycle, failed_start_needs_no_stop)
{
    /* Without a terminal notcurses_init fails; the value must remain
     * uninitialized so callers may safely skip apl_tui_stop. */
    apl_tui tui = {0};

    apl_error err;
    apl_error_init(&err);

    const apl_status status = apl_tui_start(&tui, &err);
    if (status != APL_ERR_TUI_INIT) {
        /* A terminal was actually available in this environment. */
        cr_assert_eq(status, APL_OK);
        apl_tui_stop(&tui);
        cr_assert_null(tui.notcurses);
        return;
    }

    cr_assert_null(tui.notcurses);
    cr_assert_neq(strstr(apl_error_message(&err), "Notcurses initialization failed"),
                  (char *)NULL);
}
