#include "tui/layout.h"

/** Return the smaller of two unsigned values. */
static unsigned min_u(const unsigned left, const unsigned right)
{
    return (left < right) ? left : right;
}

/** Return the larger of two unsigned values. */
static unsigned max_u(const unsigned left, const unsigned right)
{
    return (left > right) ? left : right;
}

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

/** Centered dialog rectangle inside the body for a normal-size terminal. */
static apl_tui_rect normal_overlay(const apl_tui_rect body)
{
    if (body.rows == 0 || body.cols == 0) {
        return (apl_tui_rect){};
    }
    const unsigned overlay_rows = min_u(body.rows, max_u(8, (body.rows * 4) / 5));
    const unsigned overlay_cols = min_u(body.cols, max_u(30, (body.cols * 4) / 5));
    return make_rect(body.y + (body.rows - overlay_rows) / 2,
                     body.x + (body.cols - overlay_cols) / 2, overlay_rows, overlay_cols);
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
    unsigned rail_cols =
        normal ? APL_TUI_RAIL_COLS_NORMAL : APL_TUI_RAIL_COLS_COMPACT;
    /* Never let the rail consume more than half of a narrow screen. */
    rail_cols = min_u(rail_cols, max_u(APL_TUI_MIN_COLS / 4, cols / 2));

    layout.window = make_rect(0, 0, 1, cols);
    layout.status = make_rect(rows - 1, 0, 1, cols);
    layout.tabs = make_rect(1, 0, middle_rows, rail_cols);
    layout.body = make_rect(1, rail_cols, middle_rows, cols - rail_cols);

    unsigned switcher_rows =
        normal ? APL_TUI_SWITCHER_ROWS_NORMAL : APL_TUI_SWITCHER_ROWS_COMPACT;
    if (switcher_rows >= middle_rows) {
        switcher_rows = 1;
    }
    layout.team_switcher =
        make_rect(layout.tabs.y + layout.tabs.rows - switcher_rows, 0, switcher_rows, rail_cols);

    layout.overlay = normal ? normal_overlay(layout.body) : layout.body;
    return layout;
}
