#ifndef APL_TUI_LIST_H
#define APL_TUI_LIST_H

#include <stddef.h>

/*
 * Reusable list/table viewport state (work item 2.3).
 *
 * Pure arithmetic: a screen owns the list state and turns it into rows on the
 * frame's body plane. Selection always stays visible when the viewport is
 * non-empty. Mouse/keyboard handlers call the same movement functions.
 */

typedef struct apl_tui_list {
    size_t count;
    size_t selected; /* 0 when count is 0 */
    size_t offset;   /* first visible item */
    unsigned viewport_rows;
} apl_tui_list;

/** Initialize an empty list with a viewport height. */
void apl_tui_list_init(apl_tui_list *list, unsigned viewport_rows);

/** Update item count, clamping selection/offset and keeping selection visible. */
void apl_tui_list_set_count(apl_tui_list *list, size_t count);

/** Update viewport height (for example after resize). */
void apl_tui_list_set_viewport_rows(apl_tui_list *list, unsigned rows);

/** Select an absolute index, clamped to the list. */
void apl_tui_list_select(apl_tui_list *list, size_t index);

/** Move selection by signed rows (arrows/wheel). */
void apl_tui_list_move(apl_tui_list *list, int delta);

/** Move selection by signed pages (PageUp/PageDown). */
void apl_tui_list_page(apl_tui_list *list, int pages);

/** Select the first/last item. */
void apl_tui_list_home(apl_tui_list *list);
void apl_tui_list_end(apl_tui_list *list);

/** Number of items visible in the current viewport. */
size_t apl_tui_list_visible_count(const apl_tui_list *list);

/** True when `index` is inside the current visible window. */
bool apl_tui_list_index_visible(const apl_tui_list *list, size_t index);

#endif /* APL_TUI_LIST_H */
