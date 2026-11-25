#ifndef APL_TUI_SHELL_H
#define APL_TUI_SHELL_H

#include "app/app.h"
#include "core/error.h"
#include "tui/lifecycle.h"

/*
 * Interactive application shell (Phase 2).
 *
 * Runs the shared frame and team workspace screens until the user quits.
 * Requires a started TUI and a started application (database). Navigation,
 * list selection, and dialogs are driven through the app use-case layer; the
 * shell never executes SQL.
 */

/** Run the interactive shell until the user requests exit.
 *
 * Returns APL_OK after a clean quit. APL_ERR_TUI_RENDER means a frame could not
 * be drawn; APL_ERR_TUI_INPUT means the terminal input stream became
 * unavailable. A database/use-case failure is reported with its own status.
 * The caller stops the TUI and app afterwards. */
apl_status apl_tui_shell_run(const apl_tui *tui, const apl_app *app, apl_error *error);

#endif /* APL_TUI_SHELL_H */
