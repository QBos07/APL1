#include <criterion/criterion.h>

#include "tui/dialog.h"

Test(tui_dialog, starts_inactive)
{
    apl_tui_dialog dialog;
    apl_tui_dialog_init(&dialog);

    cr_assert_not(apl_tui_dialog_active(&dialog));
    cr_assert_eq(apl_tui_dialog_activate(&dialog), APL_TUI_DIALOG_PENDING);
    cr_assert_eq(apl_tui_dialog_dismiss(&dialog), APL_TUI_DIALOG_PENDING);
}

Test(tui_dialog, confirmation_starts_on_cancel_and_requires_explicit_confirm)
{
    apl_tui_dialog dialog;
    apl_tui_dialog_open_confirm(&dialog, "Archive team", "Keep its history?");

    cr_assert(apl_tui_dialog_active(&dialog));
    cr_assert_eq(dialog.kind, APL_TUI_DIALOG_CONFIRM);
    cr_assert_eq(dialog.focus, 0);
    cr_assert_str_eq(dialog.title, "Archive team");

    cr_assert_eq(apl_tui_dialog_activate(&dialog), APL_TUI_DIALOG_DISMISSED);

    apl_tui_dialog_move_focus(&dialog, 1);
    cr_assert_eq(dialog.focus, 1);
    cr_assert_eq(apl_tui_dialog_activate(&dialog), APL_TUI_DIALOG_ACCEPTED);

    apl_tui_dialog_move_focus(&dialog, -1);
    cr_assert_eq(dialog.focus, 0);
}

Test(tui_dialog, error_dialog_has_one_dismiss_action)
{
    apl_tui_dialog dialog;
    apl_tui_dialog_open_error(&dialog, "Could not save", "Database is unavailable");

    apl_tui_dialog_move_focus(&dialog, 1);
    cr_assert_eq(dialog.focus, 0);
    cr_assert_eq(apl_tui_dialog_activate(&dialog), APL_TUI_DIALOG_DISMISSED);
    cr_assert_eq(apl_tui_dialog_dismiss(&dialog), APL_TUI_DIALOG_DISMISSED);
}
