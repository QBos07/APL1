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

apl_tui_layout apl_tui_layout_compute(const unsigned rows, const unsigned cols)
{
    apl_tui_layout layout = {};

    if (rows < APL_TUI_MIN_ROWS || cols < APL_TUI_MIN_COLS) {
        layout.mode = APL_TUI_LAYOUT_TOO_SMALL;
        return layout;
    }

    layout.mode = APL_TUI_LAYOUT_NORMAL;
    layout.header = make_rect(0, 0, 1, cols);
    layout.body = make_rect(1, 0, rows - APL_TUI_FRAME_ROWS, cols);
    layout.actions = make_rect(rows - 2, 0, 1, cols);
    layout.status = make_rect(rows - 1, 0, 1, cols);

    /* Keep the modal inside the body so the frame context and status remain
     * visible. The body is at least 7 rows, so the 5-row floor always fits. */
    const unsigned overlay_rows = min_u(layout.body.rows, max_u(7, (layout.body.rows * 3) / 4));
    const unsigned overlay_cols = min_u(layout.body.cols, max_u(24, (layout.body.cols * 3) / 4));
    layout.overlay = make_rect(layout.body.y + (layout.body.rows - overlay_rows) / 2,
                               (layout.body.cols - overlay_cols) / 2, overlay_rows, overlay_cols);

    return layout;
}
