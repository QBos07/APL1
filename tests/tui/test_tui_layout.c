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

Test(tui_layout, below_minimum_is_too_small)
{
    const apl_tui_layout sizes[] = {
        apl_tui_layout_compute(0, 0),
        apl_tui_layout_compute(APL_TUI_MIN_ROWS - 1, 200),
        apl_tui_layout_compute(200, APL_TUI_MIN_COLS - 1),
    };

    for (size_t index = 0; index < sizeof(sizes) / sizeof(sizes[0]); ++index) {
        cr_assert_eq(sizes[index].mode, APL_TUI_LAYOUT_TOO_SMALL);
        assert_rect_zero(sizes[index].header);
        assert_rect_zero(sizes[index].body);
        assert_rect_zero(sizes[index].actions);
        assert_rect_zero(sizes[index].status);
        assert_rect_zero(sizes[index].overlay);
    }
}

Test(tui_layout, minimum_size_is_normal)
{
    const apl_tui_layout layout = apl_tui_layout_compute(APL_TUI_MIN_ROWS, APL_TUI_MIN_COLS);

    cr_assert_eq(layout.mode, APL_TUI_LAYOUT_NORMAL);
    cr_assert_eq(layout.header.y, 0);
    cr_assert_eq(layout.header.rows, 1);
    cr_assert_eq(layout.header.cols, APL_TUI_MIN_COLS);
    cr_assert_eq(layout.body.y, 1);
    cr_assert_eq(layout.body.rows, APL_TUI_MIN_ROWS - APL_TUI_FRAME_ROWS);
    cr_assert_eq(layout.actions.y, APL_TUI_MIN_ROWS - 2);
    cr_assert_eq(layout.status.y, APL_TUI_MIN_ROWS - 1);
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

            cr_assert_eq(layout.mode, APL_TUI_LAYOUT_NORMAL);

            const apl_tui_rect regions[] = {layout.header, layout.body, layout.actions,
                                            layout.status};
            unsigned expected_y = 0;
            for (size_t index = 0; index < sizeof(regions) / sizeof(regions[0]); ++index) {
                const apl_tui_rect rect = regions[index];
                cr_assert_eq(rect.y, expected_y);
                cr_assert_eq(rect.x, 0);
                cr_assert_eq(rect.cols, cols);
                cr_assert_gt(rect.rows, 0);
                cr_assert_leq(rect.y + rect.rows, rows);
                expected_y += rect.rows;
            }
            cr_assert_eq(expected_y, rows);
        }
    }
}

Test(tui_layout, overlay_stays_inside_the_body)
{
    const unsigned row_counts[] = {10, 11, 24, 25, 60};
    const unsigned col_counts[] = {40, 41, 80, 81, 200};

    for (size_t r = 0; r < sizeof(row_counts) / sizeof(row_counts[0]); ++r) {
        for (size_t c = 0; c < sizeof(col_counts) / sizeof(col_counts[0]); ++c) {
            const apl_tui_layout layout = apl_tui_layout_compute(row_counts[r], col_counts[c]);

            cr_assert_eq(layout.mode, APL_TUI_LAYOUT_NORMAL);
            cr_assert_gt(layout.overlay.rows, 0);
            cr_assert_gt(layout.overlay.cols, 0);
            cr_assert_geq(layout.overlay.y, layout.body.y);
            cr_assert_geq(layout.overlay.x, layout.body.x);
            cr_assert_leq(layout.overlay.y + layout.overlay.rows, layout.body.y + layout.body.rows);
            cr_assert_leq(layout.overlay.x + layout.overlay.cols, layout.body.x + layout.body.cols);
        }
    }
}

Test(tui_layout, overlay_is_centered_with_integer_division)
{
    const apl_tui_layout layout = apl_tui_layout_compute(24, 80);

    cr_assert_eq(layout.overlay.rows, 15);
    cr_assert_eq(layout.overlay.cols, 60);
    cr_assert_eq(layout.overlay.y, layout.body.y + (layout.body.rows - layout.overlay.rows) / 2);
    cr_assert_eq(layout.overlay.x, (layout.body.cols - layout.overlay.cols) / 2);
}
