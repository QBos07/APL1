#include "tui/lifecycle.h"

#include <notcurses/notcurses.h>

apl_status apl_tui_start(apl_tui *tui, apl_error *error)
{
    if (tui == NULL) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "TUI context is NULL");
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (tui->notcurses != NULL) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "TUI is already started");
        return APL_ERR_INVALID_ARGUMENT;
    }

    notcurses_options options = {0};
    struct notcurses *context = notcurses_init(&options, NULL);
    if (context == NULL) {
        /* Partial startup: there is no context to stop, and the value stays
         * uninitialized so cleanup paths remain safe. */
        tui->notcurses = NULL;
        apl_error_set(error, APL_ERR_TUI_INIT,
                      "Notcurses initialization failed; a usable interactive terminal is required");
        return APL_ERR_TUI_INIT;
    }

    tui->notcurses = context;
    return APL_OK;
}

void apl_tui_stop(apl_tui *tui)
{
    if (tui == NULL || tui->notcurses == NULL) {
        return;
    }
    notcurses_stop(tui->notcurses);
    tui->notcurses = NULL;
}
