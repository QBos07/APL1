#include "tui/list.h"

#include <stdint.h>

#include "core/util.h"

static void ensure_selection_visible(apl_tui_list *const list)
{
    if (list->count == 0) {
        list->selected = 0;
        list->offset = 0;
        return;
    }
    if (list->selected >= list->count) {
        list->selected = list->count - 1;
    }
    if (list->viewport_rows == 0) {
        list->offset = list->selected;
        return;
    }

    if (list->selected < list->offset) {
        list->offset = list->selected;
    } else if (list->selected >= list->offset + list->viewport_rows) {
        list->offset = list->selected - list->viewport_rows + 1;
    }

    const size_t max_offset =
        (list->count > list->viewport_rows) ? list->count - list->viewport_rows : 0;
    if (list->offset > max_offset) {
        list->offset = max_offset;
    }
}

void apl_tui_list_init(apl_tui_list *const list, const unsigned viewport_rows)
{
    if (list == nullptr) {
        return;
    }
    *list = (apl_tui_list){.viewport_rows = viewport_rows};
}

void apl_tui_list_set_count(apl_tui_list *const list, const size_t count)
{
    if (list == nullptr) {
        return;
    }
    list->count = count;
    ensure_selection_visible(list);
}

void apl_tui_list_set_viewport_rows(apl_tui_list *const list, const unsigned rows)
{
    if (list == nullptr) {
        return;
    }
    list->viewport_rows = rows;
    ensure_selection_visible(list);
}

void apl_tui_list_select(apl_tui_list *const list, const size_t index)
{
    if (list == nullptr || list->count == 0) {
        return;
    }
    list->selected = apl_min_size(index, list->count - 1);
    ensure_selection_visible(list);
}

void apl_tui_list_move(apl_tui_list *const list, const int delta)
{
    if (list == nullptr || list->count == 0) {
        return;
    }
    const int64_t target = (int64_t)list->selected + delta;
    if (target <= 0) {
        list->selected = 0;
    } else if (target >= (int64_t)(list->count - 1)) {
        list->selected = list->count - 1;
    } else {
        list->selected = (size_t)target;
    }
    ensure_selection_visible(list);
}

void apl_tui_list_page(apl_tui_list *const list, const int pages)
{
    if (list == nullptr || list->count == 0) {
        return;
    }
    const int64_t step = (list->viewport_rows > 0) ? (int64_t)list->viewport_rows : 1;
    const int64_t target = (int64_t)list->selected + step * pages;
    if (target <= 0) {
        list->selected = 0;
    } else if (target >= (int64_t)(list->count - 1)) {
        list->selected = list->count - 1;
    } else {
        list->selected = (size_t)target;
    }
    ensure_selection_visible(list);
}

void apl_tui_list_home(apl_tui_list *const list)
{
    if (list == nullptr || list->count == 0) {
        return;
    }
    list->selected = 0;
    ensure_selection_visible(list);
}

void apl_tui_list_end(apl_tui_list *const list)
{
    if (list == nullptr || list->count == 0) {
        return;
    }
    list->selected = list->count - 1;
    ensure_selection_visible(list);
}

size_t apl_tui_list_visible_count(const apl_tui_list *const list)
{
    if (list == nullptr || list->count == 0) {
        return 0;
    }
    if (list->offset >= list->count) {
        return 0;
    }
    return apl_min_size((size_t)list->viewport_rows, list->count - list->offset);
}

bool apl_tui_list_index_visible(const apl_tui_list *const list, const size_t index)
{
    if (list == nullptr || list->viewport_rows == 0) {
        return false;
    }
    return index >= list->offset && index < list->offset + (size_t)list->viewport_rows &&
           index < list->count;
}
