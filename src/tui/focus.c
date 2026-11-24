#include "tui/focus.h"

static bool valid_region(const apl_tui_focus_region region)
{
    return region >= APL_TUI_FOCUS_WINDOW && region < APL_TUI_FOCUS_REGION_COUNT;
}

static bool modal_region(const apl_tui_focus_region region)
{
    return region == APL_TUI_FOCUS_COMMAND || region == APL_TUI_FOCUS_DIALOG;
}

void apl_tui_focus_init(apl_tui_focus *const focus)
{
    if (focus == nullptr) {
        return;
    }
    *focus = (apl_tui_focus){
        .region = APL_TUI_FOCUS_BODY,
        .return_region = APL_TUI_FOCUS_BODY,
        .modal = false,
    };
}

void apl_tui_focus_set(apl_tui_focus *const focus, const apl_tui_focus_region region)
{
    if (focus == nullptr || !valid_region(region) || modal_region(region)) {
        return;
    }
    focus->region = region;
}

void apl_tui_focus_push(apl_tui_focus *const focus, const apl_tui_focus_region region)
{
    if (focus == nullptr || !valid_region(region) || !modal_region(region)) {
        return;
    }
    if (!focus->modal) {
        focus->return_region = focus->region;
    }
    focus->region = region;
    focus->modal = true;
}

void apl_tui_focus_pop(apl_tui_focus *const focus)
{
    if (focus == nullptr) {
        return;
    }
    focus->region = valid_region(focus->return_region) ? focus->return_region : APL_TUI_FOCUS_BODY;
    focus->modal = false;
}

void apl_tui_focus_cycle(apl_tui_focus *const focus, const int delta)
{
    if (focus == nullptr || focus->modal || delta == 0) {
        return;
    }
    const int count = (int)APL_TUI_FOCUS_STATUS + 1;
    int index = (int)focus->region + delta;
    while (index < 0) {
        index += count;
    }
    index %= count;
    if (index > (int)APL_TUI_FOCUS_STATUS) {
        index = 0;
    }
    focus->region = (apl_tui_focus_region)index;
}

bool apl_tui_focus_modal(const apl_tui_focus *const focus)
{
    return focus != nullptr && focus->modal;
}
