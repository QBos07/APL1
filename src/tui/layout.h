#ifndef APL_TUI_LAYOUT_H
#define APL_TUI_LAYOUT_H

/*
 * Pure screen geometry for the redesigned TUI shell (work item 2.16).
 *
 * This module deliberately has no Notcurses dependency: given terminal rows
 * and columns it returns a complete set of region rectangles, so layout can
 * be unit-tested without opening an interactive terminal. The frame module
 * owns the actual planes and applies these rectangles to them.
 *
 * Layout contract:
 * - A normal frame needs at least APL_TUI_MIN_ROWS x APL_TUI_MIN_COLS cells.
 *   Below that the result is APL_TUI_LAYOUT_TOO_SMALL and every rectangle is
 *   zeroed; the caller shows a single resize notice instead of creating child
 *   planes.
 * - Normal/compact regions tile the whole terminal without overlap:
 *     window  (row 0, full width)
 *     tabs    (left column below the window bar, down to the status bar)
 *     body    (remaining area right of the tabs, same rows as the tabs)
 *     status  (bottom row, full width)
 * - The team switcher is the bottom strip of the tab rail. It never overlaps
 *   the tab rows; compact mode keeps a single visible line.
 * - The dialog overlay stays inside the body. In compact mode it fills the
 *   body so form controls remain usable at the supported minimum. It is
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
    /** Terminal is below the supported minimum; all rectangles are zero. */
    APL_TUI_LAYOUT_TOO_SMALL = 0,
    /** Narrow/short terminal: compact rail and full-body dialogs. */
    APL_TUI_LAYOUT_COMPACT,
    /** Recommended desktop terminal size. */
    APL_TUI_LAYOUT_NORMAL,
} apl_tui_layout_mode;

/** Smallest terminal the shared frame supports. 80x24 is the recommended
 *  interactive size; 40x10 is the functional minimum. */
constexpr unsigned APL_TUI_MIN_ROWS = 10;
constexpr unsigned APL_TUI_MIN_COLS = 40;

/** Normal mode starts at this size; smaller (but supported) terminals use the
 *  compact rail/dialog treatment. */
constexpr unsigned APL_TUI_NORMAL_MIN_ROWS = 18;
constexpr unsigned APL_TUI_NORMAL_MIN_COLS = 80;

/** Fixed vertical regions: window bar and status bar. */
constexpr unsigned APL_TUI_CHROME_ROWS = 2;

/** Tab-rail widths and team-switcher heights. */
constexpr unsigned APL_TUI_RAIL_COLS_NORMAL = 22;
constexpr unsigned APL_TUI_RAIL_COLS_COMPACT = 18;
constexpr unsigned APL_TUI_SWITCHER_ROWS_NORMAL = 2;
constexpr unsigned APL_TUI_SWITCHER_ROWS_COMPACT = 1;

/** Computed region rectangles for one terminal size. */
typedef struct apl_tui_layout {
    apl_tui_layout_mode mode;
    unsigned rows;
    unsigned cols;
    apl_tui_rect window;
    apl_tui_rect tabs;
    apl_tui_rect team_switcher;
    apl_tui_rect body;
    apl_tui_rect status;
    apl_tui_rect overlay;
} apl_tui_layout;

/** Compute the frame layout for `rows` x `cols` terminal cells.
 *
 * Pure: no Notcurses context, allocation, or global state. A zero dimension
 * is treated as too small (all rectangles zero). */
apl_tui_layout apl_tui_layout_compute(unsigned rows, unsigned cols);

#endif /* APL_TUI_LAYOUT_H */
