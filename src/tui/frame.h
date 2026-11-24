#ifndef APL_TUI_FRAME_H
#define APL_TUI_FRAME_H

#include "core/error.h"
#include "tui/layout.h"
#include "tui/lifecycle.h"

struct ncplane;

/*
 * Shared TUI frame (work items 2.2 and 2.16).
 *
 * Owns the persistent window/tabs/body/status planes, a caller-positioned popup
 * plane for window-bar dropdowns, and the lazily created modal overlay. The
 * standard plane remains the base surface; the only text drawn there is the
 * too-small notice. Screens borrow a region plane for one frame and never
 * resize/move/destroy it.
 */

typedef enum apl_tui_region {
    APL_TUI_REGION_WINDOW = 0,
    APL_TUI_REGION_TABS,
    APL_TUI_REGION_BODY,
    APL_TUI_REGION_STATUS,
    APL_TUI_REGION_COUNT,
} apl_tui_region;

typedef struct apl_tui_frame {
    struct notcurses *notcurses; /* borrowed from apl_tui */
    struct ncplane *planes[APL_TUI_REGION_COUNT];
    struct ncplane *popup;
    struct ncplane *overlay;
    apl_tui_layout layout;
    apl_tui_rect popup_rect;
    bool popup_open;
    bool normal;
    bool overlay_open;
} apl_tui_frame;

/** Start a frame on a started TUI. Safe to call once per frame instance;
 *  zero-initialize the value first. */
apl_status apl_tui_frame_start(apl_tui_frame *frame, const apl_tui *tui, apl_error *error);

/** Recompute geometry from the current terminal size and create/resize planes.
 *
 * Also handles the too-small transition and draws the resize notice on the
 * standard plane. Call before drawing every loop iteration, and always after
 * an NCKEY_RESIZE event. */
apl_status apl_tui_frame_sync(apl_tui_frame *frame, const apl_tui *tui, apl_error *error);

/** Erase all visible frame planes before a complete redraw. */
void apl_tui_frame_begin(apl_tui_frame *frame);

/** Borrow one region plane for the current draw; nullptr when too small or
 *  uninitialized. */
struct ncplane *apl_tui_frame_plane(const apl_tui_frame *frame, apl_tui_region region);

/** Borrow the popup plane; nullptr when no popup is open or the terminal is
 *  too small. */
struct ncplane *apl_tui_frame_popup(const apl_tui_frame *frame);

/** Borrow the modal overlay plane; nullptr when no modal is open or the
 *  terminal is too small. */
struct ncplane *apl_tui_frame_overlay(const apl_tui_frame *frame);

/** Enable/disable the modal overlay. The plane is created/destroyed by the
 *  next sync; the semantic state survives a too-small transition. */
void apl_tui_frame_set_overlay(apl_tui_frame *frame, bool open);

/** Position and show/hide the dropdown popup. The rectangle is absolute and
 *  is clamped by the caller; the plane is created/destroyed by the next sync. */
void apl_tui_frame_set_popup(apl_tui_frame *frame, bool open, apl_tui_rect rect);

/** True when the last sync produced a normal or compact (non-too-small) frame. */
bool apl_tui_frame_normal(const apl_tui_frame *frame);

/** Access the current computed layout. */
const apl_tui_layout *apl_tui_frame_layout(const apl_tui_frame *frame);

/** Render the assembled frame. */
apl_status apl_tui_frame_render(apl_tui_frame *frame, apl_error *error);

/** Destroy owned planes. Does not stop Notcurses; safe on a partial frame. */
void apl_tui_frame_stop(apl_tui_frame *frame);

/** Draw a NUL-terminated string at a plane position, clipped by the plane.
 *  A null plane/text or out-of-range row is a no-op. */
void apl_tui_frame_draw_text(struct ncplane *plane, int row, int col, const char *text);

/** Draw a line at column 0. */
void apl_tui_frame_draw_line(struct ncplane *plane, int row, const char *text);

/** Word-wrap text into rows starting at `row`, within `width` columns.
 *  Returns the number of rows written. */
unsigned apl_tui_frame_draw_wrapped(struct ncplane *plane, int row, int col, unsigned width,
                                    const char *text);

/** Set/reset the current plane style (for example NCSTYLE_BOLD). */
void apl_tui_frame_set_styles(struct ncplane *plane, unsigned styles);

#endif /* APL_TUI_FRAME_H */
