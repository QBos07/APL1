#include "tui/shell_internal.h"

static const char *dialog_value(const shell_state *const state, const unsigned index)
{
    const apl_tui_dialog_field *const field = apl_tui_dialog_field_at(&state->dialog, index);
    return field != nullptr ? field->value : "";
}

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

    if (state->dialog_context == SHELL_DIALOG_CONTEXT_PROJECT_CREATE ||
        state->dialog_context == SHELL_DIALOG_CONTEXT_PROJECT_EDIT) {
        const shell_dialog_context context = state->dialog_context;
        if (result == APL_TUI_DIALOG_DISMISSED) {
            close_shell_dialog(state);
            set_status(state, "Project cancelled without writing.");
            return;
        }
        const bool saved =
            (context == SHELL_DIALOG_CONTEXT_PROJECT_CREATE)
                ? perform_project_create(state, dialog_value(state, 0), dialog_value(state, 1),
                                         dialog_value(state, 2), dialog_value(state, 3),
                                         dialog_value(state, 4), dialog_value(state, 5),
                                         dialog_value(state, 6), dialog_value(state, 7),
                                         dialog_value(state, 8), &error)
                : perform_project_update(state, dialog_value(state, 0), dialog_value(state, 1),
                                         dialog_value(state, 2), dialog_value(state, 3),
                                         dialog_value(state, 4), dialog_value(state, 5),
                                         dialog_value(state, 6), dialog_value(state, 7),
                                         dialog_value(state, 8), &error);
        if (saved) {
            close_shell_dialog(state);
        } else if (state->dialog_context == context) {
            apl_tui_dialog_set_field_error(&state->dialog, project_form_error_field(&error),
                                           apl_error_message(&error));
        }
        return;
    }

    if (state->dialog_context == SHELL_DIALOG_CONTEXT_PROJECT_UPDATE) {
        if (result == APL_TUI_DIALOG_DISMISSED) {
            close_shell_dialog(state);
            set_status(state, "Project update cancelled without writing.");
            return;
        }
        const bool saved =
            perform_project_log_update(state, dialog_value(state, 0), dialog_value(state, 1),
                                       dialog_value(state, 2), dialog_value(state, 3), &error);
        if (saved) {
            close_shell_dialog(state);
        } else if (state->dialog_context == SHELL_DIALOG_CONTEXT_PROJECT_UPDATE) {
            apl_tui_dialog_set_field_error(&state->dialog, project_update_form_error_field(&error),
                                           apl_error_message(&error));
        }
        return;
    }

    if (state->dialog_context == SHELL_DIALOG_CONTEXT_PROJECT_FILTER) {
        if (result == APL_TUI_DIALOG_DISMISSED) {
            close_shell_dialog(state);
            set_status(state, "Project filters unchanged.");
            return;
        }
        char status_display[APL_PROJECT_STATUS_MAX];
        char status_key[APL_PROJECT_STATUS_MAX];
        apl_status filter_status = apl_project_label_normalize(
            dialog_value(state, 0), "status filter", status_display, sizeof(status_display),
            status_key, sizeof(status_key), &error);
        if (filter_status != APL_OK) {
            apl_tui_dialog_set_field_error(&state->dialog, 0, apl_error_message(&error));
            return;
        }
        int64_t season = 0;
        filter_status = project_resolve_season(state, dialog_value(state, 1), &season, &error);
        if (filter_status != APL_OK) {
            apl_tui_dialog_set_field_error(&state->dialog, 1, apl_error_message(&error));
            return;
        }
        char category_display[APL_PROJECT_CATEGORY_MAX];
        char category_key[APL_PROJECT_CATEGORY_MAX];
        filter_status = apl_project_label_normalize(dialog_value(state, 2), "category filter",
                                                    category_display, sizeof(category_display),
                                                    category_key, sizeof(category_key), &error);
        if (filter_status != APL_OK) {
            apl_tui_dialog_set_field_error(&state->dialog, 2, apl_error_message(&error));
            return;
        }
        (void)snprintf(state->project_status_filter, sizeof(state->project_status_filter), "%s",
                       status_key);
        state->project_season_filter = season;
        (void)snprintf(state->project_category_filter, sizeof(state->project_category_filter), "%s",
                       category_key);
        close_shell_dialog(state);
        if (load_projects(state, 0, &error) != APL_OK) {
            show_error_dialog(state, &error);
            return;
        }
        set_status(state, "Project filters applied.");
        return;
    }

    /* Parts dialogs (Phase 6). */
    if (state->dialog_context == SHELL_DIALOG_CONTEXT_PART_TYPE_CREATE ||
        state->dialog_context == SHELL_DIALOG_CONTEXT_PART_TYPE_EDIT) {
        const shell_dialog_context context = state->dialog_context;
        if (result == APL_TUI_DIALOG_DISMISSED) {
            close_shell_dialog(state);
            set_status(state, "Part type cancelled without writing.");
            return;
        }
        const bool saved =
            (context == SHELL_DIALOG_CONTEXT_PART_TYPE_CREATE)
                ? perform_part_type_create(state, dialog_value(state, 0), dialog_value(state, 1),
                                           dialog_value(state, 2), dialog_value(state, 3), &error)
                : perform_part_type_update(state, dialog_value(state, 0), dialog_value(state, 1),
                                           dialog_value(state, 2), dialog_value(state, 3), &error);
        if (saved) {
            close_shell_dialog(state);
        } else if (state->dialog_context == context) {
            apl_tui_dialog_set_field_error(&state->dialog, part_type_form_error_field(&error),
                                           apl_error_message(&error));
        }
        return;
    }

    if (state->dialog_context == SHELL_DIALOG_CONTEXT_PART_UNIT_CREATE ||
        state->dialog_context == SHELL_DIALOG_CONTEXT_PART_UNIT_EDIT) {
        const shell_dialog_context context = state->dialog_context;
        if (result == APL_TUI_DIALOG_DISMISSED) {
            close_shell_dialog(state);
            set_status(state, "Part unit cancelled without writing.");
            return;
        }
        if (context == SHELL_DIALOG_CONTEXT_PART_UNIT_CREATE) {
            int64_t type_id = 0;
            apl_status resolve_status =
                part_resolve_type_id_for_dialog(state, dialog_value(state, 0), &type_id, &error);
            if (resolve_status != APL_OK) {
                apl_tui_dialog_set_field_error(&state->dialog, 0, apl_error_message(&error));
                return;
            }
            const bool saved =
                perform_part_unit_create(state, dialog_value(state, 0), dialog_value(state, 1),
                                         dialog_value(state, 2), dialog_value(state, 3),
                                         dialog_value(state, 4), &error);
            if (saved) {
                close_shell_dialog(state);
            } else if (state->dialog_context == context) {
                apl_tui_dialog_set_field_error(&state->dialog, part_unit_form_error_field(&error),
                                               apl_error_message(&error));
            }
        } else {
            const bool saved =
                perform_part_unit_update(state, state->dialog_part_type_id, dialog_value(state, 1),
                                         dialog_value(state, 2), dialog_value(state, 3),
                                         dialog_value(state, 4), &error);
            if (saved) {
                close_shell_dialog(state);
            } else if (state->dialog_context == context) {
                apl_tui_dialog_set_field_error(&state->dialog, part_unit_form_error_field(&error),
                                               apl_error_message(&error));
            }
        }
        return;
    }

    if (state->dialog_context == SHELL_DIALOG_CONTEXT_PART_FILTER) {
        if (result == APL_TUI_DIALOG_DISMISSED) {
            close_shell_dialog(state);
            set_status(state, "Parts filters unchanged.");
            return;
        }
        if (!apply_parts_filter(state, dialog_value(state, 0), "", dialog_value(state, 1),
                                dialog_value(state, 2), dialog_value(state, 3), &error)) {
            apl_tui_dialog_set_field_error(&state->dialog, 0, apl_error_message(&error));
            return;
        }
        close_shell_dialog(state);
        return;
    }

    if (state->dialog_context == SHELL_DIALOG_CONTEXT_PART_INSTALL ||
        state->dialog_context == SHELL_DIALOG_CONTEXT_PART_REMOVE ||
        state->dialog_context == SHELL_DIALOG_CONTEXT_PART_REPAIR ||
        state->dialog_context == SHELL_DIALOG_CONTEXT_PART_DISPOSE) {
        apl_lifecycle_action action = APL_LIFECYCLE_INSTALL;
        if (state->dialog_context == SHELL_DIALOG_CONTEXT_PART_REMOVE) {
            action = APL_LIFECYCLE_REMOVE;
        } else if (state->dialog_context == SHELL_DIALOG_CONTEXT_PART_REPAIR) {
            action = APL_LIFECYCLE_REPAIR;
        } else if (state->dialog_context == SHELL_DIALOG_CONTEXT_PART_DISPOSE) {
            action = APL_LIFECYCLE_DISPOSE;
        }
        if (result == APL_TUI_DIALOG_DISMISSED) {
            close_shell_dialog(state);
            set_status(state, "Lifecycle action cancelled; nothing was written.");
            return;
        }
        if (!perform_lifecycle_action(state, action, dialog_value(state, 0),
                                      dialog_value(state, 1), dialog_value(state, 2),
                                      dialog_value(state, 3), dialog_value(state, 4),
                                      dialog_value(state, 5), dialog_value(state, 6),
                                      dialog_value(state, 7), &error)) {
            apl_tui_dialog_set_field_error(&state->dialog, lifecycle_form_error_field(&error),
                                           apl_error_message(&error));
            return;
        }
        close_shell_dialog(state);
        return;
    }

    if (state->dialog_context == SHELL_DIALOG_CONTEXT_PART_REPLACE) {
        if (result == APL_TUI_DIALOG_DISMISSED) {
            close_shell_dialog(state);
            set_status(state, "Replacement cancelled; no change was saved.");
            return;
        }
        if (result == APL_TUI_DIALOG_ACCEPTED) {
            /* Store entered values and open an explicit review before the
             * atomic save. Cancelling the review writes nothing. */
            (void)snprintf(state->replace_old_unit, sizeof(state->replace_old_unit), "%s",
                           dialog_value(state, 0));
            (void)snprintf(state->replace_new_unit, sizeof(state->replace_new_unit), "%s",
                           dialog_value(state, 1));
            (void)snprintf(state->replace_car, sizeof(state->replace_car), "%s",
                           dialog_value(state, 2));
            (void)snprintf(state->replace_position, sizeof(state->replace_position), "%s",
                           dialog_value(state, 3));
            (void)snprintf(state->replace_date, sizeof(state->replace_date), "%s",
                           dialog_value(state, 4));
            (void)snprintf(state->replace_event, sizeof(state->replace_event), "%s",
                           dialog_value(state, 5));
            (void)snprintf(state->replace_reason, sizeof(state->replace_reason), "%s",
                           dialog_value(state, 6));
            (void)snprintf(state->replace_usage, sizeof(state->replace_usage), "%s",
                           dialog_value(state, 7));
            (void)snprintf(state->replace_notes, sizeof(state->replace_notes), "%s",
                           dialog_value(state, 8));
            close_shell_dialog(state);
            state->pending = SHELL_PENDING_APPLY_REPLACE;
            char message[APL_TUI_DIALOG_MESSAGE_MAX];
            (void)snprintf(message, sizeof(message),
                           "Replace old unit %s with new unit %s%s%s on %s%s%s. Confirm to save "
                           "atomically; the old unit's history and the new unit remain inspectable.",
                           state->replace_old_unit, state->replace_new_unit,
                           state->replace_car[0] != '\0' ? " at car " : "",
                           state->replace_car[0] != '\0' ? state->replace_car : "",
                           state->replace_date,
                           state->replace_position[0] != '\0' ? " position " : "",
                           state->replace_position[0] != '\0' ? state->replace_position : "");
            apl_tui_dialog_open_confirm(&state->dialog, "Confirm replacement", message);
            apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
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
        if (state->pending_ledger_kind != APL_LEDGER_KIND_OPENING_BALANCE) {
            (void)apl_tui_dialog_set_field_value(&state->dialog, 4, state->pending_project);
        }
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
        (void)apl_tui_dialog_set_field_value(&state->dialog, 4, state->pending_budget_project);
        (void)apl_tui_dialog_set_field_value(&state->dialog, 5, state->pending_notes);
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
    } else if (pending == SHELL_PENDING_ARCHIVE_PROJECT) {
        if (result == APL_TUI_DIALOG_ACCEPTED) {
            perform_project_archive(state, pending_record, &error);
        } else {
            set_status(state, "Archive cancelled.");
        }
    } else if (pending == SHELL_PENDING_ARCHIVE_BUDGET) {
        if (result == APL_TUI_DIALOG_ACCEPTED) {
            perform_budget_archive(state, pending_record, &error);
        } else {
            set_status(state, "Archive cancelled.");
        }
    } else if (pending == SHELL_PENDING_ARCHIVE_PART_TYPE) {
        if (result == APL_TUI_DIALOG_ACCEPTED) {
            perform_part_type_archive(state, pending_record, &error);
        } else {
            set_status(state, "Part type archive cancelled.");
        }
    } else if (pending == SHELL_PENDING_ARCHIVE_PART_UNIT) {
        if (result == APL_TUI_DIALOG_ACCEPTED) {
            perform_part_unit_archive(state, pending_record, &error);
        } else {
            set_status(state, "Part unit archive cancelled.");
        }
    } else if (pending == SHELL_PENDING_APPLY_REPLACE) {
        if (result == APL_TUI_DIALOG_ACCEPTED) {
            if (!perform_replace(state, state->replace_old_unit, state->replace_new_unit,
                                 state->replace_car, state->replace_position, state->replace_date,
                                 state->replace_event, state->replace_reason, state->replace_usage,
                                 state->replace_notes, &error)) {
                if (state->dialog_context == SHELL_DIALOG_CONTEXT_NONE) {
                    show_error_dialog(state, &error);
                }
            }
        } else {
            set_status(state, "Replacement cancelled; no change was saved.");
        }
    } else if (result == APL_TUI_DIALOG_DISMISSED) {
        set_status(state, "Cancelled.");
    }
}
