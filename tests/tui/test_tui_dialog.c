#include <limits.h>
#include <string.h>

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

Test(tui_dialog, form_collects_values_and_requires_required_fields)
{
    apl_tui_dialog dialog;
    apl_tui_dialog_open_form(&dialog, "Create team", "Required fields are marked.");
    cr_assert_eq(apl_tui_dialog_add_field(&dialog, "Name", "", true), 0);
    cr_assert_eq(apl_tui_dialog_add_field(&dialog, "Notes", "Existing note", false), 1);

    cr_assert(apl_tui_dialog_focus_is_field(&dialog));
    apl_tui_dialog_insert_text(&dialog, "Fer");
    apl_tui_dialog_insert_text(&dialog, "rari");
    cr_assert_str_eq(dialog.fields[0].value, "Ferrari");
    cr_assert_str_eq(dialog.fields[1].value, "Existing note");

    apl_tui_dialog_move_focus(&dialog, 1);
    cr_assert_eq(apl_tui_dialog_focused_field(&dialog), 1);
    apl_tui_dialog_move_focus(&dialog, 1);
    cr_assert_eq(dialog.focus, 2); /* Save */
    apl_tui_dialog_move_focus(&dialog, 1);
    cr_assert_eq(dialog.focus, 3); /* Cancel */
    cr_assert_eq(apl_tui_dialog_activate(&dialog), APL_TUI_DIALOG_DISMISSED);

    /* A missing required value keeps the dialog open and preserves input. */
    apl_tui_dialog_open_form(&dialog, "Create team", "");
    (void)apl_tui_dialog_add_field(&dialog, "Name", "", true);
    cr_assert_eq(apl_tui_dialog_activate(&dialog), APL_TUI_DIALOG_PENDING);
    cr_assert(apl_tui_dialog_active(&dialog));
    cr_assert_eq(dialog.focus, 0);
    cr_assert(dialog.fields[0].invalid);
    cr_assert_neq(dialog.validation[0], '\0');
}

Test(tui_dialog, form_business_error_keeps_values_and_focuses_the_field)
{
    apl_tui_dialog dialog;
    apl_tui_dialog_open_form(&dialog, "Edit team", "");
    (void)apl_tui_dialog_add_field(&dialog, "Name", "Ferrari", true);
    (void)apl_tui_dialog_add_field(&dialog, "Notes", "Maranello", false);

    apl_tui_dialog_set_field_error(&dialog, 0, "A team named 'Ferrari' already exists.");
    cr_assert_eq(dialog.focus, 0);
    cr_assert(dialog.fields[0].invalid);
    cr_assert_str_eq(dialog.fields[1].value, "Maranello");
    cr_assert_str_eq(dialog.validation, "A team named 'Ferrari' already exists.");

    apl_tui_dialog_insert_text(&dialog, "!");
    cr_assert_not(dialog.fields[0].invalid);
    cr_assert_str_eq(dialog.fields[0].value, "Ferrari!");
    cr_assert_str_eq(dialog.fields[1].value, "Maranello");
}

Test(tui_dialog, choice_skips_disabled_options_and_cannot_activate_them)
{
    apl_tui_dialog dialog;
    apl_tui_dialog_open_choice(&dialog, "Switch team", "Select an active team.");
    (void)apl_tui_dialog_add_option(&dialog, "Ferrari", true, true);
    (void)apl_tui_dialog_add_option(&dialog, "Old team [archived]", false, false);
    (void)apl_tui_dialog_add_option(&dialog, "McLaren", true, false);

    cr_assert_eq(apl_tui_dialog_selected_option(&dialog), 0);
    apl_tui_dialog_move_focus(&dialog, 1);
    cr_assert_eq(apl_tui_dialog_selected_option(&dialog), 2);
    apl_tui_dialog_move_focus(&dialog, 1);
    cr_assert_eq(apl_tui_dialog_selected_option(&dialog), 0);
    apl_tui_dialog_move_focus(&dialog, -1);
    cr_assert_eq(apl_tui_dialog_selected_option(&dialog), 2);
    cr_assert_eq(apl_tui_dialog_activate(&dialog), APL_TUI_DIALOG_ACCEPTED);
}

Test(tui_dialog, choice_scrolls_a_long_option_list)
{
    apl_tui_dialog dialog;
    apl_tui_dialog_open_choice(&dialog, "Teams", "");
    for (unsigned index = 0; index < 12; ++index) {
        (void)apl_tui_dialog_add_option(&dialog, "Team", true, false);
    }
    apl_tui_dialog_set_viewport_rows(&dialog, 4);
    for (unsigned index = 0; index < 8; ++index) {
        apl_tui_dialog_move_focus(&dialog, 1);
    }
    cr_assert_eq(dialog.focus, 8);
    cr_assert_eq(dialog.option_offset, 5);
    cr_assert_leq(dialog.focus, dialog.option_offset + 4 - 1);
}

Test(tui_dialog, hit_testing_matches_drawn_control_rows)
{
    apl_tui_dialog dialog;
    apl_tui_dialog_open_form(&dialog, "Create", "");
    (void)apl_tui_dialog_add_field(&dialog, "Name", "", true);
    (void)apl_tui_dialog_add_field(&dialog, "Notes", "", false);

    cr_assert_eq(apl_tui_dialog_control_at(&dialog, 12, 40, 3, 2), 0);
    cr_assert_eq(apl_tui_dialog_control_at(&dialog, 12, 40, 4, 2), 1);
    cr_assert_eq(apl_tui_dialog_control_at(&dialog, 12, 40, 10, 3), 2);  /* Save */
    cr_assert_eq(apl_tui_dialog_control_at(&dialog, 12, 40, 10, 15), 3); /* Cancel */
    cr_assert_eq(apl_tui_dialog_control_at(&dialog, 12, 40, 10, 30), UINT_MAX);

    apl_tui_dialog_open_choice(&dialog, "Teams", "");
    (void)apl_tui_dialog_add_option(&dialog, "Ferrari", true, true);
    (void)apl_tui_dialog_add_option(&dialog, "McLaren", true, false);
    cr_assert_eq(apl_tui_dialog_control_at(&dialog, 12, 40, 3, 2), 0);
    cr_assert_eq(apl_tui_dialog_control_at(&dialog, 12, 40, 4, 2), 1);
    cr_assert_eq(apl_tui_dialog_control_at(&dialog, 12, 40, 5, 2), UINT_MAX);
}
