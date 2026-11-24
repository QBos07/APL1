#include "tui/shell.h"

#include <notcurses/notcurses.h>

/* Screen text for the temporary scaffold shell. Deliberately small: the
 * first-run/team flow belongs to Phase 2 (see work item 1.9). */

/** Draw one line at `row`, truncated to the plane width. Rows beyond the
 *  plane are skipped so a tiny terminal cannot produce out-of-area drawing. */
static void draw_line(struct ncplane *const plane, const int row, const char *const text)
{
    unsigned rows = 0;
    unsigned cols = 0;
    ncplane_dim_yx(plane, &rows, &cols);
    if (row < 0 || (unsigned)row >= rows) {
        return;
    }
    (void)ncplane_printf_yx(plane, row, 0, "%.*s", (int)cols, text);
}

static void draw_screen(struct ncplane *const plane)
{
    constexpr int title_row = 0;
    constexpr int empty_state_row = 2;
    constexpr int exit_hint_row = 4;

    draw_line(plane, title_row, "apl1 - Formula 1 Team Management Database");
    draw_line(plane, empty_state_row,
              "No teams have been added yet. Management screens arrive in later phases.");
    draw_line(plane, exit_hint_row, "Press q to quit.");
}

apl_status apl_tui_shell_run(const apl_tui *const tui, apl_error *const error)
{
    if (tui == nullptr || tui->notcurses == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "TUI is not started");
        return APL_ERR_INVALID_ARGUMENT;
    }

    struct notcurses *const nc = tui->notcurses;
    struct ncplane *const plane = notcurses_stdplane(nc);

    for (;;) {
        ncplane_erase(plane);
        draw_screen(plane);

        if (notcurses_render(nc) != 0) {
            apl_error_set(error, APL_ERR_TUI_RENDER, "screen rendering failed");
            return APL_ERR_TUI_RENDER;
        }

        ncinput input = {};
        const uint32_t id = notcurses_get_blocking(nc, &input);
        if (id == (uint32_t)-1) {
            /* The input stream failed: without input the shell cannot be
             * operated, so report instead of spinning forever. */
            apl_error_set(error, APL_ERR_TUI_INPUT, "terminal input became unavailable");
            return APL_ERR_TUI_INPUT;
        }
        if (id == NCKEY_EOF) {
            /* The input stream reached EOF (e.g. the terminal closed): the
             * same report, since the shell cannot be operated without it. */
            apl_error_set(error, APL_ERR_TUI_INPUT, "terminal input reached EOF");
            return APL_ERR_TUI_INPUT;
        }
        if (id == 'q' || id == 'Q') {
            return APL_OK;
        }
        /* Any other key, including NCKEY_RESIZE, redraws from the current
         * plane dimensions on the next iteration. */
    }
}
