#include <stdio.h>

#include "app/app.h"
#include "app/exit_code.h"
#include "core/error.h"
#include "tui/lifecycle.h"
#include "tui/shell.h"

/*
 * The interactive main loop stays a thin composition: application setup,
 * TUI run, cleanup (work item 1.10). All failure handling is reporting;
 * cleanup always runs through apl_tui_stop and apl_app_stop. Process exit
 * codes come from the stable mapping in app/exit_code.h (FH-11).
 */
int main()
{
    apl_error error;
    apl_error_init(&error);

    /* TUI first: a launch without a usable interactive terminal must fail
     * cleanly before any database file is created. */
    apl_tui tui = {};
    apl_status status = apl_tui_start(&tui, &error);
    if (status != APL_OK) {
        fprintf(stderr, "apl1: %s\n", apl_error_message(&error));
        return (int)apl_exit_code_for_status(status);
    }

    const apl_app_config config = {}; /* default database path from the environment */
    apl_app app = {};
    status = apl_app_start(&app, &config, &error);
    if (status != APL_OK) {
        apl_tui_stop(&tui);
        fprintf(stderr, "apl1: %s\n", apl_error_message(&error));
        return (int)apl_exit_code_for_status(status);
    }

    status = apl_tui_shell_run(&tui, &error);
    apl_app_stop(&app);
    apl_tui_stop(&tui);

    if (status != APL_OK) {
        fprintf(stderr, "apl1: %s\n", apl_error_message(&error));
        return (int)apl_exit_code_for_status(status);
    }
    return (int)apl_exit_code_for_status(status);
}
