#ifndef APL_TUI_LAYOUT_H
#define APL_TUI_LAYOUT_H

/*
 * Pure screen geometry for the shared TUI frame (work item 2.0.1).
 *
 * This module deliberately has no Notcurses dependency: given terminal rows
 * and columns it returns a complete set of region rectangles, so layout can
 * be unit-tested without opening an interactive terminal. The frame module
 * (2.2) owns the actual planes and applies these rectangles to them.
 *
 * Layout contract:
 * - A normal frame needs at least APL_TUI_MIN_ROWS x APL_TUI_MIN_COLS cells.
 *   Below that the result is APL_TUI_LAYOUT_TOO_SMALL and every rectangle is
 *   zeroed; the caller shows a single resize notice instead of creating child
 *   planes.
 * - Normal regions tile the whole terminal without overlap:
 *     header  (row 0)
 *     body    (rows 1 .. rows-3)
 *     actions (row rows-2)
 *     status  (row rows-1)
 * - The overlay rectangle is centered inside the body region so the frame's
 *   header, actions, and status stay visible while a modal is open. It is
 *   computed even when no modal is active; callers use it only on demand.
 * - All coordinates are absolute terminal cells. No caller may draw outside
 *   its assigned rectangle.
 */

/** A non-negative rectangle in terminal cells. */
typedef struct apl_tui_rect {
    unsigned y;
    unsigned x;
    unsigned rows;
    unsigned cols;
} apl_tui_rect;

/** How the terminal dimensions map to the shared frame. */
typedef enum apl_tui_layout_mode {
    /** All frame rectangles are populated and tile the terminal. */
    APL_TUI_LAYOUT_NORMAL,
    /** Terminal is below the supported minimum; all rectangles are zero. */
    APL_TUI_LAYOUT_TOO_SMALL,
} apl_tui_layout_mode;

/** Smallest terminal the shared frame supports. Below this the frame cannot
 *  show a title line, one content row, an action line, and a status line.
 *  80x24 is the recommended interactive size; 40x10 is the functional
 *  minimum. */
constexpr unsigned APL_TUI_MIN_ROWS = 10;
constexpr unsigned APL_TUI_MIN_COLS = 40;

/** Fixed frame rows: header, actions, and status. */
constexpr unsigned APL_TUI_FRAME_ROWS = 3;

/** Computed region rectangles for one terminal size. */
typedef struct apl_tui_layout {
    apl_tui_layout_mode mode;
    apl_tui_rect header;
    apl_tui_rect body;
    apl_tui_rect actions;
    apl_tui_rect status;
    apl_tui_rect overlay;
} apl_tui_layout;

/** Compute the frame layout for `rows` x `cols` terminal cells.
 *
 * Pure: no Notcurses context, allocation, or global state. A zero dimension
 * is treated as too small (all rectangles zero). */
apl_tui_layout apl_tui_layout_compute(unsigned rows, unsigned cols);

#endif /* APL_TUI_LAYOUT_H */
