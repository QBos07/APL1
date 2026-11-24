#ifndef APL_TUI_LIFECYCLE_H
#define APL_TUI_LIFECYCLE_H

#include "core/error.h"

struct notcurses; /* Notcurses Core context; only src/tui sources need the type. */

/*
 * Notcurses initialize/stop lifecycle.
 *
 * Ownership: an apl_tui value owns exactly one `struct notcurses` context once
 * apl_tui_start succeeds, and releases it in apl_tui_stop. Stop is safe on an
 * uninitialized or already-stopped value (a partial or failed startup never
 * leaves a context that stop would mishandle), and a context is stopped at
 * most once. Normal exit and error exit routes both call the same
 * apl_tui_stop.
 *
 * Screen drawing and input handling stay out of this module.
 */

/** Owns the Notcurses context. Zero-initialize before apl_tui_start. */
typedef struct apl_tui {
    struct notcurses *notcurses;
} apl_tui;

/** Initialize Notcurses Core for the current terminal.
 *
 * A usable interactive terminal is required: stdin and stdout must both be
 * terminal devices. On failure (no terminal, or Notcurses initialization
 * failure) `error` (optional) receives APL_ERR_TUI_INIT and the value stays
 * uninitialized; no stop call is required. Notcurses options are the
 * defaults: this is a local terminal application with no special
 * configuration yet.
 */
apl_status apl_tui_start(apl_tui *tui, apl_error *error);

/** Stop Notcurses and restore the terminal to a usable state.
 *
 * Safe on an uninitialized/zero-initialized value and on an already-stopped
 * value; each successful stop runs exactly once.
 */
void apl_tui_stop(apl_tui *tui);

#endif /* APL_TUI_LIFECYCLE_H */
