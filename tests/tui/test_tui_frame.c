#include <string.h>

#include <criterion/criterion.h>

#include "tui/frame.h"

/*
 * Terminal-free frame contracts only. Real plane creation/resize/render is
 * exercised by the interactive smoke test.
 */

Test(tui_frame, stop_on_uninitialized_frame_is_safe)
{
    apl_tui_frame frame = {};
    apl_tui_frame_stop(&frame);
    apl_tui_frame_stop(&frame);
    cr_assert_null(frame.notcurses);
}

Test(tui_frame, start_requires_a_started_tui)
{
    apl_tui tui = {};
    apl_tui_frame frame = {};

    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_tui_frame_start(&frame, &tui, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_null(frame.notcurses);
    cr_assert_neq(strstr(apl_error_message(&err), "started TUI"), nullptr);
}

Test(tui_frame, accessors_on_uninitialized_frame_are_safe)
{
    apl_tui_frame frame = {};

    cr_assert_null(apl_tui_frame_plane(&frame, APL_TUI_REGION_BODY));
    cr_assert_null(apl_tui_frame_plane(&frame, APL_TUI_REGION_WINDOW));
    cr_assert_null(apl_tui_frame_popup(&frame));
    cr_assert_null(apl_tui_frame_overlay(&frame));
    cr_assert_not(apl_tui_frame_normal(&frame));
    cr_assert_null(apl_tui_frame_layout(&frame));

    apl_tui_frame_begin(&frame); /* no-op */
    apl_tui_frame_set_overlay(&frame, true);
    apl_tui_frame_set_popup(&frame, true, (apl_tui_rect){.y = 1, .x = 1, .rows = 3, .cols = 20});
    cr_assert_null(apl_tui_frame_overlay(&frame));
    cr_assert_null(apl_tui_frame_popup(&frame));
}

Test(tui_frame, popup_requires_a_non_empty_rectangle)
{
    apl_tui_frame frame = {};
    apl_tui_frame_set_popup(&frame, true, (apl_tui_rect){});
    cr_assert_not(frame.popup_open);

    apl_tui_frame_set_popup(&frame, true, (apl_tui_rect){.y = 1, .x = 1, .rows = 3, .cols = 20});
    cr_assert(frame.popup_open);

    apl_tui_frame_set_popup(&frame, false, (apl_tui_rect){.y = 1, .x = 1, .rows = 3, .cols = 20});
    cr_assert_not(frame.popup_open);
}
