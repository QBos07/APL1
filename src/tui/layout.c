#include "tui/layout.h"

#include "core/util.h"

/** Build a rectangle from already validated non-negative values. */
static apl_tui_rect make_rect(const unsigned y, const unsigned x, const unsigned rows,
                              const unsigned cols)
{
    return (apl_tui_rect){
        .y = y,
        .x = x,
        .rows = rows,
        .cols = cols,
    };
}

/** Dialog rectangle inside the body. Dialogs fill the body so underlying body
 *  text cannot collide with the first content line; the window bar, tab rail,
 *  team switcher, and status bar remain visible around it. */
static apl_tui_rect overlay_rect(const apl_tui_rect body)
{
    return body;
}

apl_tui_layout apl_tui_layout_compute(const unsigned rows, const unsigned cols)
{
    apl_tui_layout layout = {
        .rows = rows,
        .cols = cols,
    };

    if (rows < APL_TUI_MIN_ROWS || cols < APL_TUI_MIN_COLS) {
        layout.mode = APL_TUI_LAYOUT_TOO_SMALL;
        return layout;
    }

    const bool normal = rows >= APL_TUI_NORMAL_MIN_ROWS && cols >= APL_TUI_NORMAL_MIN_COLS;
    layout.mode = normal ? APL_TUI_LAYOUT_NORMAL : APL_TUI_LAYOUT_COMPACT;

    const unsigned middle_rows = rows - APL_TUI_CHROME_ROWS;
    unsigned rail_cols = normal ? APL_TUI_RAIL_COLS_NORMAL : APL_TUI_RAIL_COLS_COMPACT;
    /* Never let the rail consume more than half of a narrow screen. */
    rail_cols = apl_min_unsigned(rail_cols, apl_max_unsigned(APL_TUI_MIN_COLS / 4, cols / 2));

    layout.window = make_rect(0, 0, 1, cols);
    layout.status = make_rect(rows - 1, 0, 1, cols);
    layout.tabs = make_rect(1, 0, middle_rows, rail_cols);
    layout.body = make_rect(1, rail_cols, middle_rows, cols - rail_cols);

    unsigned switcher_rows = normal ? APL_TUI_SWITCHER_ROWS_NORMAL : APL_TUI_SWITCHER_ROWS_COMPACT;
    if (switcher_rows >= middle_rows) {
        switcher_rows = 1;
    }
    layout.team_switcher =
        make_rect(layout.tabs.y + layout.tabs.rows - switcher_rows, 0, switcher_rows, rail_cols);

    layout.overlay = overlay_rect(layout.body);
    return layout;
}
