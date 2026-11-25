#include "tui/shell_internal.h"

void handle_dialog_input(shell_state *const state, const ncinput *const input)
{
    const uint32_t id = input->id;
    if (id == NCKEY_LEFT || id == NCKEY_UP) {
        apl_tui_dialog_move_focus(&state->dialog, -1);
        return;
    }
    if (id == NCKEY_RIGHT || id == NCKEY_DOWN) {
        apl_tui_dialog_move_focus(&state->dialog, 1);
        return;
    }
    if (id == NCKEY_TAB) {
        apl_tui_dialog_move_focus(&state->dialog, ncinput_shift_p(input) ? -1 : 1);
        return;
    }
    if (id == NCKEY_BACKSPACE || id == 127) {
        apl_tui_dialog_backspace(&state->dialog);
        return;
    }
    if (id >= 0x20 && id < PRETERUNICODEBASE && input->utf8[0] != '\0') {
        apl_tui_dialog_insert_text(&state->dialog, input->utf8);
        return;
    }

    apl_tui_dialog_result result = APL_TUI_DIALOG_PENDING;
    if (is_enter_key(id)) {
        result = apl_tui_dialog_activate(&state->dialog);
    } else if (id == NCKEY_ESC) {
        result = apl_tui_dialog_dismiss(&state->dialog);
    }
    if (result == APL_TUI_DIALOG_PENDING) {
        return;
    }

    apl_error error;
    apl_error_init(&error);

    if (state->dialog_context == SHELL_DIALOG_CONTEXT_TEAM_PICKER) {
        finish_team_picker(state, result);
        return;
    }
    if (state->dialog_context == SHELL_DIALOG_CONTEXT_DRIVER_PICKER) {
        finish_driver_picker(state, result);
        return;
    }
    if (state->dialog_context == SHELL_DIALOG_CONTEXT_ACTIVITY_DETAIL) {
        close_shell_dialog(state);
        return;
    }

    if (state->dialog_context >= SHELL_DIALOG_CONTEXT_SEASON_CREATE &&
        state->dialog_context <= SHELL_DIALOG_CONTEXT_MEMBERSHIP_END) {
        const shell_dialog_context context = state->dialog_context;
        if (result == APL_TUI_DIALOG_DISMISSED) {
            close_shell_dialog(state);
            set_status(state, "Cancelled without writing.");
            return;
        }
        bool saved = false;
        if (context == SHELL_DIALOG_CONTEXT_SEASON_CREATE ||
            context == SHELL_DIALOG_CONTEXT_SEASON_EDIT) {
            const apl_tui_dialog_field *const label = apl_tui_dialog_field_at(&state->dialog, 0);
            const apl_tui_dialog_field *const start = apl_tui_dialog_field_at(&state->dialog, 1);
            const apl_tui_dialog_field *const end = apl_tui_dialog_field_at(&state->dialog, 2);
            const apl_tui_dialog_field *const notes = apl_tui_dialog_field_at(&state->dialog, 3);
            saved = (context == SHELL_DIALOG_CONTEXT_SEASON_CREATE)
                        ? perform_season_create(state, label ? label->value : "",
                                                start ? start->value : "", end ? end->value : "",
                                                notes ? notes->value : "", &error)
                        : perform_season_update(state, label ? label->value : "",
                                                start ? start->value : "", end ? end->value : "",
                                                notes ? notes->value : "", &error);
        } else if (context == SHELL_DIALOG_CONTEXT_CAR_CREATE ||
                   context == SHELL_DIALOG_CONTEXT_CAR_EDIT) {
            const apl_tui_dialog_field *const label = apl_tui_dialog_field_at(&state->dialog, 0);
            const apl_tui_dialog_field *const chassis = apl_tui_dialog_field_at(&state->dialog, 1);
            const apl_tui_dialog_field *const notes = apl_tui_dialog_field_at(&state->dialog, 2);
            saved = (context == SHELL_DIALOG_CONTEXT_CAR_CREATE)
                        ? perform_car_create(state, label ? label->value : "",
                                             chassis ? chassis->value : "",
                                             notes ? notes->value : "", &error)
                        : perform_car_update(state, label ? label->value : "",
                                             chassis ? chassis->value : "",
                                             notes ? notes->value : "", &error);
        } else if (context == SHELL_DIALOG_CONTEXT_DRIVER_CREATE ||
                   context == SHELL_DIALOG_CONTEXT_DRIVER_EDIT) {
            const apl_tui_dialog_field *const name = apl_tui_dialog_field_at(&state->dialog, 0);
            const apl_tui_dialog_field *const short_name =
                apl_tui_dialog_field_at(&state->dialog, 1);
            const apl_tui_dialog_field *const notes = apl_tui_dialog_field_at(&state->dialog, 2);
            saved = (context == SHELL_DIALOG_CONTEXT_DRIVER_CREATE)
                        ? perform_driver_create(state, name ? name->value : "",
                                                short_name ? short_name->value : "",
                                                notes ? notes->value : "", &error)
                        : perform_driver_update(state, name ? name->value : "",
                                                short_name ? short_name->value : "",
                                                notes ? notes->value : "", &error);
        } else if (context == SHELL_DIALOG_CONTEXT_MEMBERSHIP_ADD) {
            const apl_tui_dialog_field *const start = apl_tui_dialog_field_at(&state->dialog, 0);
            const apl_tui_dialog_field *const end = apl_tui_dialog_field_at(&state->dialog, 1);
            const apl_tui_dialog_field *const role = apl_tui_dialog_field_at(&state->dialog, 2);
            const apl_tui_dialog_field *const notes = apl_tui_dialog_field_at(&state->dialog, 3);
            saved =
                perform_membership_add(state, start ? start->value : "", end ? end->value : "",
                                       role ? role->value : "", notes ? notes->value : "", &error);
        } else if (context == SHELL_DIALOG_CONTEXT_MEMBERSHIP_END) {
            const apl_tui_dialog_field *const end = apl_tui_dialog_field_at(&state->dialog, 1);
            saved = perform_membership_end(state, end ? end->value : "", &error);
        }
        if (saved) {
            close_shell_dialog(state);
        } else if (state->dialog_context == context) {
            apl_tui_dialog_set_field_error(&state->dialog,
                                           reference_form_error_field(context, &error),
                                           apl_error_message(&error));
        }
        return;
    }

    if (state->dialog_context == SHELL_DIALOG_CONTEXT_MONEY_CURRENCY) {
        if (result == APL_TUI_DIALOG_DISMISSED) {
            close_shell_dialog(state);
            set_status(state, "Currency change cancelled.");
            return;
        }
        const apl_tui_dialog_field *const field = apl_tui_dialog_field_at(&state->dialog, 0);
        apl_team updated = {};
        const apl_status status =
            apl_team_service_set_currency(state->app, state->dialog_team_id,
                                          field != nullptr ? field->value : "", &updated, &error);
        if (status != APL_OK) {
            set_status_from_error(state, "Cannot set currency", &error);
            apl_tui_dialog_set_field_error(&state->dialog, 0, apl_error_message(&error));
            return;
        }
        if (state->detail.id == updated.id) {
            state->detail = updated;
        }
        close_shell_dialog(state);
        if (refresh_active(state, &error) != APL_OK ||
            refresh_teams(state, updated.id, &error) != APL_OK) {
            show_error_dialog(state, &error);
            return;
        }
        if (state->section == APL_TUI_SECTION_MONEY) {
            const apl_status reload = (state->screen == SHELL_SCREEN_BUDGET_LIST)
                                          ? load_budget_report(state, &error)
                                          : load_ledger(state, 0, &error);
            if (reload != APL_OK) {
                show_error_dialog(state, &error);
                return;
            }
        }
        set_status_success(state, "Reporting currency is now %s.", updated.currency);
        return;
    }

    if (state->dialog_context == SHELL_DIALOG_CONTEXT_MONEY_ENTRY) {
        if (result == APL_TUI_DIALOG_DISMISSED) {
            close_shell_dialog(state);
            set_status(state, "Cancelled without writing.");
            return;
        }
        store_money_entry_values(state);
        open_money_review_dialog(state);
        return;
    }

    if (state->dialog_context == SHELL_DIALOG_CONTEXT_MONEY_CORRECT) {
        if (result == APL_TUI_DIALOG_DISMISSED) {
            close_shell_dialog(state);
            set_status(state, "Correction cancelled.");
            return;
        }
        store_money_correction_values(state);
        open_money_correction_review(state);
        return;
    }

    if (state->dialog_context == SHELL_DIALOG_CONTEXT_MONEY_FILTER) {
        if (result == APL_TUI_DIALOG_DISMISSED) {
            close_shell_dialog(state);
            set_status(state, "Filter unchanged.");
            return;
        }
        const apl_tui_dialog_field *const field = apl_tui_dialog_field_at(&state->dialog, 0);
        char key[APL_LEDGER_CATEGORY_MAX] = "";
        if (field != nullptr && field->value[0] != '\0') {
            char label[APL_LEDGER_CATEGORY_MAX];
            if (apl_ledger_category_normalize(field->value, label, sizeof(label), key, sizeof(key),
                                              &error) != APL_OK) {
                apl_tui_dialog_set_field_error(&state->dialog, 0, apl_error_message(&error));
                return;
            }
        }
        const bool budgets = state->screen == SHELL_SCREEN_BUDGET_LIST;
        if (budgets) {
            (void)snprintf(state->budget_category_filter, sizeof(state->budget_category_filter),
                           "%s", key);
        } else {
            (void)snprintf(state->money_category_filter, sizeof(state->money_category_filter), "%s",
                           key);
        }
        close_shell_dialog(state);
        const apl_status reload =
            budgets ? load_budget_report(state, &error) : load_ledger(state, 0, &error);
        if (reload != APL_OK) {
            show_error_dialog(state, &error);
            return;
        }
        set_status(state, key[0] != '\0' ? "Category filter: %s." : "Category filter cleared.",
                   key);
        return;
    }

    if (state->dialog_context == SHELL_DIALOG_CONTEXT_BUDGET_FORM) {
        if (result == APL_TUI_DIALOG_DISMISSED) {
            close_shell_dialog(state);
            set_status(state, "Budget plan unchanged.");
            return;
        }
        store_budget_values(state);
        open_budget_review_dialog(state);
        return;
    }

    if (state->dialog_context == SHELL_DIALOG_CONTEXT_MONEY_REVIEW) {
        if (result == APL_TUI_DIALOG_DISMISSED) {
            close_shell_dialog(state);
            set_status(state, "Cancelled without writing.");
            return;
        }
        const bool saved = perform_money_post(state, &error);
        if (saved) {
            close_shell_dialog(state);
            return;
        }
        const unsigned field = money_form_error_field(&error);
        close_shell_dialog(state);
        open_money_entry_dialog(state, state->pending_ledger_kind);
        (void)apl_tui_dialog_set_field_value(&state->dialog, 0, state->pending_amount);
        (void)apl_tui_dialog_set_field_value(&state->dialog, 1, state->pending_date);
        (void)apl_tui_dialog_set_field_value(&state->dialog, 2, state->pending_category);
        (void)apl_tui_dialog_set_field_value(&state->dialog, 3, state->pending_description);
        apl_tui_dialog_set_field_error(&state->dialog, field, apl_error_message(&error));
        return;
    }

    if (state->dialog_context == SHELL_DIALOG_CONTEXT_MONEY_CORRECT_REVIEW) {
        if (result == APL_TUI_DIALOG_DISMISSED) {
            close_shell_dialog(state);
            set_status(state, "Correction cancelled.");
            return;
        }
        const bool saved = perform_money_correction(state, &error);
        if (saved) {
            close_shell_dialog(state);
            return;
        }
        const unsigned field = money_form_error_field(&error);
        const size_t index = find_ledger_index(state, state->dialog_record_id);
        const apl_ledger_entry *const original =
            (index != SIZE_MAX) ? &state->entries[index] : &state->entry_detail;
        if (original->id == 0) {
            close_shell_dialog(state);
            show_error_dialog(state, &error);
            return;
        }
        close_shell_dialog(state);
        open_money_correct_dialog(state, original, state->pending_amount, state->pending_date,
                                  state->pending_category, state->pending_description);
        apl_tui_dialog_set_field_error(&state->dialog, field, apl_error_message(&error));
        return;
    }

    if (state->dialog_context == SHELL_DIALOG_CONTEXT_BUDGET_REVIEW) {
        if (result == APL_TUI_DIALOG_DISMISSED) {
            close_shell_dialog(state);
            set_status(state, "Budget plan cancelled.");
            return;
        }
        const bool saved = perform_budget_save(state, &error);
        if (saved) {
            close_shell_dialog(state);
            return;
        }
        const unsigned field = budget_form_error_field(&error);
        const bool edit = state->dialog_record_id > 0;
        close_shell_dialog(state);
        open_budget_form(state, edit);
        (void)apl_tui_dialog_set_field_value(&state->dialog, 0, state->pending_start);
        (void)apl_tui_dialog_set_field_value(&state->dialog, 1, state->pending_end);
        (void)apl_tui_dialog_set_field_value(&state->dialog, 2, state->pending_category);
        (void)apl_tui_dialog_set_field_value(&state->dialog, 3, state->pending_amount);
        (void)apl_tui_dialog_set_field_value(&state->dialog, 4, state->pending_notes);
        apl_tui_dialog_set_field_error(&state->dialog, field, apl_error_message(&error));
        return;
    }

    if (state->dialog_context == SHELL_DIALOG_CONTEXT_TEAM_CREATE ||
        state->dialog_context == SHELL_DIALOG_CONTEXT_TEAM_EDIT) {
        const shell_dialog_context context = state->dialog_context;
        if (result == APL_TUI_DIALOG_DISMISSED) {
            close_shell_dialog(state);
            set_status(state, context == SHELL_DIALOG_CONTEXT_TEAM_CREATE
                                  ? "Team creation cancelled."
                                  : "Team edit cancelled.");
            return;
        }
        const apl_tui_dialog_field *const name_field = apl_tui_dialog_field_at(&state->dialog, 0);
        const apl_tui_dialog_field *const notes_field = apl_tui_dialog_field_at(&state->dialog, 1);
        const char *const name = (name_field != nullptr) ? name_field->value : "";
        const char *const notes = (notes_field != nullptr) ? notes_field->value : "";
        bool saved = false;
        if (context == SHELL_DIALOG_CONTEXT_TEAM_CREATE) {
            saved = perform_create(state, name, notes, &error);
        } else {
            saved = perform_update(state, state->dialog_team_id, name, notes, &error);
        }
        if (saved) {
            close_shell_dialog(state);
        } else if (state->dialog_context == context) {
            apl_tui_dialog_set_field_error(&state->dialog, form_error_field(&error),
                                           apl_error_message(&error));
        }
        return;
    }

    const shell_pending_action pending = state->pending;
    const int64_t pending_team = state->pending_team_id;
    const int64_t pending_record = state->pending_record_id;
    state->pending = SHELL_PENDING_NONE;
    state->pending_team_id = 0;
    state->pending_record_id = 0;
    close_shell_dialog(state);

    if (pending == SHELL_PENDING_ARCHIVE_TEAM) {
        if (result == APL_TUI_DIALOG_ACCEPTED) {
            perform_archive(state, pending_team, &error);
        } else {
            set_status(state, "Archive cancelled.");
        }
    } else if (pending == SHELL_PENDING_ARCHIVE_SEASON) {
        if (result == APL_TUI_DIALOG_ACCEPTED) {
            perform_season_archive(state, pending_record, &error);
        } else {
            set_status(state, "Archive cancelled.");
        }
    } else if (pending == SHELL_PENDING_ARCHIVE_CAR) {
        if (result == APL_TUI_DIALOG_ACCEPTED) {
            perform_car_archive(state, pending_record, &error);
        } else {
            set_status(state, "Archive cancelled.");
        }
    } else if (pending == SHELL_PENDING_ARCHIVE_DRIVER) {
        if (result == APL_TUI_DIALOG_ACCEPTED) {
            perform_driver_archive(state, pending_record, &error);
        } else {
            set_status(state, "Archive cancelled.");
        }
    } else if (result == APL_TUI_DIALOG_DISMISSED) {
        set_status(state, "Cancelled.");
    }
}
