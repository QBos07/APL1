#include "tui/frame.h"

#include <notcurses/notcurses.h>
#include <stdio.h>
#include <string.h>

#include "tui/text.h"

constexpr size_t WRAP_BUFFER_SIZE = 1024;

static const char *const REGION_NAMES[APL_TUI_REGION_COUNT] = {
    "window",
    "tabs",
    "body",
    "status",
};

/** Return the rectangle for one persistent region. */
static apl_tui_rect region_rect(const apl_tui_layout *const layout, const apl_tui_region region)
{
    switch (region) {
    case APL_TUI_REGION_WINDOW:
        return layout->window;
    case APL_TUI_REGION_TABS:
        return layout->tabs;
    case APL_TUI_REGION_BODY:
        return layout->body;
    case APL_TUI_REGION_STATUS:
        return layout->status;
    case APL_TUI_REGION_COUNT:
        break;
    }
    return (apl_tui_rect){};
}

/** Destroy every child plane but keep the semantic popup/overlay flags. */
static void destroy_planes(apl_tui_frame *const frame)
{
    for (size_t index = 0; index < APL_TUI_REGION_COUNT; ++index) {
        if (frame->planes[index] != nullptr) {
            ncplane_destroy(frame->planes[index]);
            frame->planes[index] = nullptr;
        }
    }
    if (frame->popup != nullptr) {
        ncplane_destroy(frame->popup);
        frame->popup = nullptr;
    }
    if (frame->overlay != nullptr) {
        ncplane_destroy(frame->overlay);
        frame->overlay = nullptr;
    }
}

/** Create one child plane at a rectangle and give it an opaque space base. */
static apl_status create_plane(struct ncplane *const standard, struct ncplane **const out,
                               const apl_tui_rect rect, const char *const name,
                               apl_error *const error)
{
    ncplane_options options = {
        .y = (int)rect.y,
        .x = (int)rect.x,
        .rows = rect.rows,
        .cols = rect.cols,
        .name = name,
    };
    struct ncplane *const plane = ncplane_create(standard, &options);
    if (plane == nullptr) {
        apl_error_set(error, APL_ERR_TUI_RENDER, "cannot create the %s plane", name);
        return APL_ERR_TUI_RENDER;
    }
    if (ncplane_set_base(plane, " ", 0, 0) < 0) {
        ncplane_destroy(plane);
        apl_error_set(error, APL_ERR_TUI_RENDER, "cannot initialize the %s plane", name);
        return APL_ERR_TUI_RENDER;
    }
    *out = plane;
    return APL_OK;
}

/** Create a missing plane or resize/move an existing one to the rectangle. */
static apl_status place_plane(apl_tui_frame *const frame, struct ncplane **const plane,
                              const apl_tui_rect rect, const char *const name, const bool top,
                              apl_error *const error)
{
    if (*plane == nullptr) {
        const apl_status status =
            create_plane(notcurses_stdplane(frame->notcurses), plane, rect, name, error);
        if (status != APL_OK) {
            return status;
        }
    } else if (ncplane_resize_simple(*plane, rect.rows, rect.cols) != 0 ||
               ncplane_move_yx(*plane, (int)rect.y, (int)rect.x) != 0) {
        apl_error_set(error, APL_ERR_TUI_RENDER, "cannot resize the %s plane", name);
        return APL_ERR_TUI_RENDER;
    }

    if (top) {
        (void)ncplane_move_top(*plane);
    }
    return APL_OK;
}

/** Draw the only standard-plane text: the too-small resize notice. */
static void draw_too_small_notice(apl_tui_frame *const frame, const unsigned rows,
                                  const unsigned cols)
{
    struct ncplane *const standard = notcurses_stdplane(frame->notcurses);
    ncplane_erase(standard);
    if (rows == 0 || cols == 0) {
        return;
    }
    char notice[160];
    (void)snprintf(notice, sizeof(notice),
                   "Terminal too small: need at least %ux%u, have %ux%u. Resize or press q.",
                   APL_TUI_MIN_COLS, APL_TUI_MIN_ROWS, cols, rows);
    apl_tui_frame_draw_line(standard, 0, notice);
}

apl_status apl_tui_frame_start(apl_tui_frame *const frame, const apl_tui *const tui,
                               apl_error *const error)
{
    if (frame == nullptr || tui == nullptr || tui->notcurses == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "TUI frame requires a started TUI");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *frame = (apl_tui_frame){.notcurses = tui->notcurses};
    return apl_tui_frame_sync(frame, tui, error);
}

apl_status apl_tui_frame_sync(apl_tui_frame *const frame, const apl_tui *const tui,
                              apl_error *const error)
{
    if (frame == nullptr || frame->notcurses == nullptr || tui == nullptr ||
        tui->notcurses != frame->notcurses) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "TUI frame is not started");
        return APL_ERR_INVALID_ARGUMENT;
    }

    unsigned rows = 0;
    unsigned cols = 0;
    (void)notcurses_stddim_yx(frame->notcurses, &rows, &cols);
    frame->layout = apl_tui_layout_compute(rows, cols);

    if (frame->layout.mode == APL_TUI_LAYOUT_TOO_SMALL) {
        destroy_planes(frame);
        draw_too_small_notice(frame, rows, cols);
        frame->normal = false;
        return APL_OK;
    }

    if (!frame->normal) {
        /* Remove the too-small notice/previous standard-plane content before
         * the persistent planes cover the screen. */
        ncplane_erase(notcurses_stdplane(frame->notcurses));
    }

    for (size_t index = 0; index < APL_TUI_REGION_COUNT; ++index) {
        const apl_tui_region region = (apl_tui_region)index;
        const apl_status status =
            place_plane(frame, &frame->planes[index], region_rect(&frame->layout, region),
                        REGION_NAMES[index], false, error);
        if (status != APL_OK) {
            return status;
        }
    }

    if (frame->popup_open) {
        const apl_status status =
            place_plane(frame, &frame->popup, frame->popup_rect, "popup", true, error);
        if (status != APL_OK) {
            return status;
        }
    } else if (frame->popup != nullptr) {
        ncplane_destroy(frame->popup);
        frame->popup = nullptr;
    }

    if (frame->overlay_open) {
        const apl_status status =
            place_plane(frame, &frame->overlay, frame->layout.overlay, "overlay", true, error);
        if (status != APL_OK) {
            return status;
        }
    } else if (frame->overlay != nullptr) {
        ncplane_destroy(frame->overlay);
        frame->overlay = nullptr;
    }

    frame->normal = true;
    return APL_OK;
}

void apl_tui_frame_begin(apl_tui_frame *const frame)
{
    if (frame == nullptr || !frame->normal) {
        return;
    }
    for (size_t index = 0; index < APL_TUI_REGION_COUNT; ++index) {
        if (frame->planes[index] != nullptr) {
            ncplane_erase(frame->planes[index]);
        }
    }
    if (frame->popup != nullptr) {
        ncplane_erase(frame->popup);
    }
    if (frame->overlay != nullptr) {
        ncplane_erase(frame->overlay);
    }
}

struct ncplane *apl_tui_frame_plane(const apl_tui_frame *const frame, const apl_tui_region region)
{
    if (frame == nullptr || !frame->normal || region < APL_TUI_REGION_WINDOW ||
        region >= APL_TUI_REGION_COUNT) {
        return nullptr;
    }
    return frame->planes[region];
}

struct ncplane *apl_tui_frame_popup(const apl_tui_frame *const frame)
{
    if (frame == nullptr || !frame->normal || !frame->popup_open) {
        return nullptr;
    }
    return frame->popup;
}

struct ncplane *apl_tui_frame_overlay(const apl_tui_frame *const frame)
{
    if (frame == nullptr || !frame->normal || !frame->overlay_open) {
        return nullptr;
    }
    return frame->overlay;
}

void apl_tui_frame_set_overlay(apl_tui_frame *const frame, const bool open)
{
    if (frame != nullptr) {
        frame->overlay_open = open;
    }
}

void apl_tui_frame_set_popup(apl_tui_frame *const frame, const bool open, const apl_tui_rect rect)
{
    if (frame == nullptr) {
        return;
    }
    const bool usable = rect.rows > 0 && rect.cols > 0;
    frame->popup_open = open && usable;
    frame->popup_rect = usable ? rect : (apl_tui_rect){};
}

bool apl_tui_frame_normal(const apl_tui_frame *const frame)
{
    return frame != nullptr && frame->normal;
}

const apl_tui_layout *apl_tui_frame_layout(const apl_tui_frame *const frame)
{
    return (frame != nullptr && frame->notcurses != nullptr) ? &frame->layout : nullptr;
}

apl_status apl_tui_frame_render(apl_tui_frame *const frame, apl_error *const error)
{
    if (frame == nullptr || frame->notcurses == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "TUI frame is not started");
        return APL_ERR_INVALID_ARGUMENT;
    }
    if (notcurses_render(frame->notcurses) != 0) {
        apl_error_set(error, APL_ERR_TUI_RENDER, "screen rendering failed");
        return APL_ERR_TUI_RENDER;
    }
    return APL_OK;
}

void apl_tui_frame_stop(apl_tui_frame *const frame)
{
    if (frame == nullptr) {
        return;
    }
    destroy_planes(frame);
    frame->overlay_open = false;
    frame->popup_open = false;
    frame->normal = false;
    frame->notcurses = nullptr;
}

void apl_tui_frame_draw_text(struct ncplane *const plane, const int row, const int col,
                             const char *const text)
{
    if (plane == nullptr || text == nullptr || row < 0 || col < 0) {
        return;
    }
    unsigned rows = 0;
    unsigned cols = 0;
    ncplane_dim_yx(plane, &rows, &cols);
    if ((unsigned)row >= rows || (unsigned)col >= cols) {
        return;
    }

    /* Clip to the remaining row. Without this, ncplane_putstr_yx can continue
     * onto following rows on terminals where a long value reaches the edge,
     * which shows up as duplicated dialog lines. */
    char clipped[WRAP_BUFFER_SIZE];
    (void)snprintf(clipped, sizeof(clipped), "%s", text);
    const size_t available = (unsigned)col < cols ? cols - (unsigned)col : 0;
    const size_t max_bytes = (available < sizeof(clipped) - 1) ? available : sizeof(clipped) - 1;
    (void)apl_tui_text_truncate_utf8(clipped, max_bytes);
    (void)ncplane_putstr_yx(plane, row, col, clipped);
}

void apl_tui_frame_draw_line(struct ncplane *const plane, const int row, const char *const text)
{
    apl_tui_frame_draw_text(plane, row, 0, text);
}

/** Flush the current wrapped line, returning the next row. */
static int flush_wrapped_line(struct ncplane *const plane, const int row, const int col,
                              const char *const line)
{
    apl_tui_frame_draw_text(plane, row, col, line);
    return row + 1;
}

unsigned apl_tui_frame_draw_wrapped(struct ncplane *const plane, const int row, const int col,
                                    const unsigned width, const char *const text)
{
    if (plane == nullptr || text == nullptr || width == 0) {
        return 0;
    }
    unsigned plane_rows = 0;
    ncplane_dim_yx(plane, &plane_rows, nullptr);
    if (row < 0 || (unsigned)row >= plane_rows) {
        return 0;
    }

    char line[WRAP_BUFFER_SIZE] = "";
    size_t line_length = 0;
    int line_width = 0;
    unsigned used = 0;
    int current_row = row;
    const char *cursor = text;

    while (*cursor != '\0' && (unsigned)current_row < plane_rows) {
        while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' || *cursor == '\r') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }

        const char *const word_start = cursor;
        while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t' && *cursor != '\n' &&
               *cursor != '\r') {
            ++cursor;
        }
        size_t word_length = (size_t)(cursor - word_start);
        if (word_length >= sizeof(line)) {
            word_length = sizeof(line) - 1;
        }
        char word[WRAP_BUFFER_SIZE];
        memcpy(word, word_start, word_length);
        word[word_length] = '\0';
        int word_width = ncstrwidth(word, nullptr, nullptr);
        if (word_width < 0) {
            word_width = 0;
        }

        if (line_length > 0 && line_width + 1 + word_width > (int)width) {
            current_row = flush_wrapped_line(plane, current_row, col, line);
            ++used;
            line[0] = '\0';
            line_length = 0;
            line_width = 0;
            if ((unsigned)current_row >= plane_rows) {
                break;
            }
        }

        if (word_width > (int)width) {
            apl_tui_frame_draw_text(plane, current_row, col, word);
            ++current_row;
            ++used;
            line[0] = '\0';
            line_length = 0;
            line_width = 0;
            continue;
        }

        if (line_length + (line_length > 0 ? 1 : 0) + word_length >= sizeof(line)) {
            current_row = flush_wrapped_line(plane, current_row, col, line);
            ++used;
            line[0] = '\0';
            line_length = 0;
            line_width = 0;
            if ((unsigned)current_row >= plane_rows) {
                break;
            }
        }

        if (line_length > 0) {
            line[line_length] = ' ';
            ++line_length;
            ++line_width;
        }
        memcpy(line + line_length, word, word_length);
        line_length += word_length;
        line[line_length] = '\0';
        line_width += word_width;
    }

    if (line_length > 0 && (unsigned)current_row < plane_rows) {
        (void)flush_wrapped_line(plane, current_row, col, line);
        ++used;
    }
    return used;
}

void apl_tui_frame_set_styles(struct ncplane *const plane, const unsigned styles)
{
    if (plane != nullptr) {
        ncplane_set_styles(plane, styles);
    }
}
