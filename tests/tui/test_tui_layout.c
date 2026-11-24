#include <criterion/criterion.h>

#include "tui/layout.h"

/*
 * Pure geometry tests: no Notcurses context or terminal is involved.
 */

static void assert_rect_zero(const apl_tui_rect rect)
{
    cr_assert_eq(rect.y, 0);
    cr_assert_eq(rect.x, 0);
    cr_assert_eq(rect.rows, 0);
    cr_assert_eq(rect.cols, 0);
}

static void assert_inside(const apl_tui_rect inner, const apl_tui_rect outer)
{
    cr_assert_geq(inner.y, outer.y);
    cr_assert_geq(inner.x, outer.x);
    cr_assert_leq(inner.y + inner.rows, outer.y + outer.rows);
    cr_assert_leq(inner.x + inner.cols, outer.x + outer.cols);
}

Test(tui_layout, below_minimum_is_too_small)
{
    const apl_tui_layout sizes[] = {
        apl_tui_layout_compute(0, 0),
        apl_tui_layout_compute(APL_TUI_MIN_ROWS - 1, 200),
        apl_tui_layout_compute(200, APL_TUI_MIN_COLS - 1),
    };

    for (size_t index = 0; index < sizeof(sizes) / sizeof(sizes[0]); ++index) {
        cr_assert_eq(sizes[index].mode, APL_TUI_LAYOUT_TOO_SMALL);
        assert_rect_zero(sizes[index].window);
        assert_rect_zero(sizes[index].tabs);
        assert_rect_zero(sizes[index].team_switcher);
        assert_rect_zero(sizes[index].body);
        assert_rect_zero(sizes[index].status);
        assert_rect_zero(sizes[index].overlay);
    }
}

Test(tui_layout, minimum_size_is_compact)
{
    const apl_tui_layout layout = apl_tui_layout_compute(APL_TUI_MIN_ROWS, APL_TUI_MIN_COLS);

    cr_assert_eq(layout.mode, APL_TUI_LAYOUT_COMPACT);
    cr_assert_eq(layout.rows, APL_TUI_MIN_ROWS);
    cr_assert_eq(layout.cols, APL_TUI_MIN_COLS);
    cr_assert_eq(layout.window.y, 0);
    cr_assert_eq(layout.window.rows, 1);
    cr_assert_eq(layout.window.cols, APL_TUI_MIN_COLS);
    cr_assert_eq(layout.tabs.y, 1);
    cr_assert_eq(layout.tabs.rows, APL_TUI_MIN_ROWS - APL_TUI_CHROME_ROWS);
    cr_assert_eq(layout.body.y, 1);
    cr_assert_eq(layout.body.cols + layout.tabs.cols, APL_TUI_MIN_COLS);
    cr_assert_eq(layout.status.y, APL_TUI_MIN_ROWS - 1);
    cr_assert_eq(layout.team_switcher.rows, APL_TUI_SWITCHER_ROWS_COMPACT);
    cr_assert_eq(layout.overlay.y, layout.body.y);
    cr_assert_eq(layout.overlay.rows, layout.body.rows);
    cr_assert_eq(layout.overlay.cols, layout.body.cols);
}

Test(tui_layout, normal_and_compact_modes_are_distinct)
{
    cr_assert_eq(apl_tui_layout_compute(APL_TUI_NORMAL_MIN_ROWS, APL_TUI_NORMAL_MIN_COLS).mode,
                 APL_TUI_LAYOUT_NORMAL);
    cr_assert_eq(apl_tui_layout_compute(APL_TUI_NORMAL_MIN_ROWS - 1, APL_TUI_NORMAL_MIN_COLS).mode,
                 APL_TUI_LAYOUT_COMPACT);
    cr_assert_eq(apl_tui_layout_compute(APL_TUI_NORMAL_MIN_ROWS, APL_TUI_NORMAL_MIN_COLS - 1).mode,
                 APL_TUI_LAYOUT_COMPACT);
}

Test(tui_layout, regions_tile_the_terminal_without_overlap)
{
    const unsigned row_counts[] = {10, 11, 23, 24, 25, 60};
    const unsigned col_counts[] = {40, 41, 79, 80, 81, 200};

    for (size_t r = 0; r < sizeof(row_counts) / sizeof(row_counts[0]); ++r) {
        for (size_t c = 0; c < sizeof(col_counts) / sizeof(col_counts[0]); ++c) {
            const unsigned rows = row_counts[r];
            const unsigned cols = col_counts[c];
            const apl_tui_layout layout = apl_tui_layout_compute(rows, cols);

            cr_assert_neq(layout.mode, APL_TUI_LAYOUT_TOO_SMALL);

            cr_assert_eq(layout.window.y, 0);
            cr_assert_eq(layout.window.x, 0);
            cr_assert_eq(layout.window.rows, 1);
            cr_assert_eq(layout.window.cols, cols);

            cr_assert_eq(layout.tabs.y, 1);
            cr_assert_eq(layout.tabs.x, 0);
            cr_assert_gt(layout.tabs.cols, 0);
            cr_assert_gt(layout.tabs.rows, 0);

            cr_assert_eq(layout.body.y, 1);
            cr_assert_eq(layout.body.x, layout.tabs.cols);
            cr_assert_gt(layout.body.cols, 0);
            cr_assert_eq(layout.body.rows, layout.tabs.rows);
            cr_assert_eq(layout.body.cols + layout.tabs.cols, cols);
            cr_assert_eq(layout.body.rows + APL_TUI_CHROME_ROWS, rows);

            cr_assert_eq(layout.status.y, rows - 1);
            cr_assert_eq(layout.status.x, 0);
            cr_assert_eq(layout.status.rows, 1);
            cr_assert_eq(layout.status.cols, cols);

            assert_inside(layout.team_switcher, layout.tabs);
            cr_assert_eq(layout.team_switcher.y + layout.team_switcher.rows,
                         layout.tabs.y + layout.tabs.rows);
        }
    }
}

Test(tui_layout, body_receives_the_remaining_space)
{
    for (unsigned cols = APL_TUI_MIN_COLS; cols <= 200; ++cols) {
        const apl_tui_layout layout = apl_tui_layout_compute(24, cols);
        cr_assert_eq(layout.body.cols + layout.tabs.cols, cols);
        cr_assert_gt(layout.body.cols, layout.tabs.cols / 2);
    }
}

Test(tui_layout, overlay_stays_inside_the_body)
{
    const unsigned row_counts[] = {10, 11, 17, 18, 24, 25, 60};
    const unsigned col_counts[] = {40, 41, 79, 80, 81, 200};

    for (size_t r = 0; r < sizeof(row_counts) / sizeof(row_counts[0]); ++r) {
        for (size_t c = 0; c < sizeof(col_counts) / sizeof(col_counts[0]); ++c) {
            const apl_tui_layout layout = apl_tui_layout_compute(row_counts[r], col_counts[c]);

            cr_assert_neq(layout.mode, APL_TUI_LAYOUT_TOO_SMALL);
            cr_assert_gt(layout.overlay.rows, 0);
            cr_assert_gt(layout.overlay.cols, 0);
            assert_inside(layout.overlay, layout.body);
        }
    }
}

Test(tui_layout, normal_overlay_is_centered_and_compact_overlay_fills_body)
{
    const apl_tui_layout normal = apl_tui_layout_compute(24, 80);
    cr_assert_eq(normal.mode, APL_TUI_LAYOUT_NORMAL);
    cr_assert_eq(normal.overlay.y, normal.body.y + (normal.body.rows - normal.overlay.rows) / 2);
    cr_assert_eq(normal.overlay.x, normal.body.x + (normal.body.cols - normal.overlay.cols) / 2);

    const apl_tui_layout compact = apl_tui_layout_compute(17, 80);
    cr_assert_eq(compact.mode, APL_TUI_LAYOUT_COMPACT);
    cr_assert_eq(compact.overlay.y, compact.body.y);
    cr_assert_eq(compact.overlay.rows, compact.body.rows);
    cr_assert_eq(compact.overlay.cols, compact.body.cols);
}
