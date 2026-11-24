#ifndef APL_TUI_SHELL_H
#define APL_TUI_SHELL_H

#include "core/error.h"
#include "tui/lifecycle.h"

/*
 * Temporary scaffold shell (work item 1.9).
 *
 * Shows the application name, an honest empty state, and a visible exit
 * affordance, then waits for the exit key. Any other key, including the
 * resize event, triggers a redraw from the current plane dimensions, so the
 * screen never draws outside the available area.
 *
 * Navigation, team management, and command entry are deliberately out of
 * scope; the first-run/team flow belongs to Phase 2.
 */

/** Run the scaffold shell until the user requests exit.
 *
 * Requires a started TUI; on a nullptr/unstarted value returns
 * APL_ERR_INVALID_ARGUMENT without touching the terminal. Returns APL_OK
 * after the user pressed the exit key. APL_ERR_TUI_RENDER means a frame
 * could not be drawn; APL_ERR_TUI_INPUT means the terminal input stream
 * became unavailable (without input the shell cannot be operated). After
 * any return the caller stops the TUI through apl_tui_stop.
 */
apl_status apl_tui_shell_run(const apl_tui *tui, apl_error *error);

#endif /* APL_TUI_SHELL_H */
