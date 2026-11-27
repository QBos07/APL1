#include "tui/shell_internal.h"

void open_create_dialog(shell_state *const state, const char *const name, const char *const notes)
{
    apl_tui_dialog_open_form(&state->dialog, "Create team",
                             "Name is required and must be unique. Notes are optional.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Name", name, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes (optional)", notes, false);
    state->dialog_context = SHELL_DIALOG_CONTEXT_TEAM_CREATE;
    state->dialog_team_id = 0;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter saves; Esc cancels without writing.");
}

void open_create_form(shell_state *const state)
{
    open_create_dialog(state, "", "");
}

void open_edit_form(shell_state *const state)
{
    apl_tui_dialog_open_form(&state->dialog, "Edit team",
                             "Editing team. Enter saves; Esc cancels.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Name", state->detail.name, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes (optional)", state->detail.notes, false);
    state->dialog_context = SHELL_DIALOG_CONTEXT_TEAM_EDIT;
    state->dialog_team_id = state->detail.id;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter saves; Esc cancels without writing.");
}

bool perform_create(shell_state *const state, const char *const name, const char *const notes,
                    apl_error *const error)
{
    apl_team created = {};
    const apl_status status = apl_team_service_create(state->app, name, notes, &created, error);
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot create team", error);
        return false;
    }

    state->any_teams = true;
    state->detail = created;
    state->selected_season_id = 0;
    apl_error_init(error);
    apl_status reload = refresh_active(state, error);
    if (reload == APL_OK) {
        reload = refresh_teams(state, created.id, error);
    }
    if (reload != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    state->section = APL_TUI_SECTION_TEAMS;
    apl_tui_list_select(&state->tabs, apl_tui_section_tab_index(APL_TUI_SECTION_TEAMS));
    sync_status_context(state);
    state->screen = SHELL_SCREEN_TEAM_DETAIL;
    set_status_success(state,
                       "Team created. Next, consider adding people, seasons, money, or events in "
                       "later phases. Press Esc for the team list.");
    return true;
}

bool perform_update(shell_state *const state, const int64_t team_id, const char *const name,
                    const char *const notes, apl_error *const error)
{
    apl_team updated = {};
    const apl_status status =
        apl_team_service_update(state->app, team_id, name, notes, &updated, error);
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot save team", error);
        return false;
    }

    state->detail = updated;
    apl_error_init(error);
    apl_status reload = refresh_active(state, error);
    if (reload == APL_OK) {
        reload = refresh_teams(state, updated.id, error);
    }
    if (reload != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    state->section = APL_TUI_SECTION_TEAMS;
    apl_tui_list_select(&state->tabs, apl_tui_section_tab_index(APL_TUI_SECTION_TEAMS));
    sync_status_context(state);
    state->screen = SHELL_SCREEN_TEAM_DETAIL;
    set_status_success(state, "Changes saved.");
    return true;
}

void perform_archive(shell_state *const state, const int64_t team_id, apl_error *const error)
{
    const bool was_active =
        active_status_has_team(state->active_status) && state->active_team.id == team_id;
    apl_team archived = {};
    const apl_status status = apl_team_service_archive(state->app, team_id, &archived, error);
    if (status != APL_OK) {
        show_error_dialog(state, error);
        return;
    }

    if (was_active) {
        state->selected_season_id = 0;
    }
    apl_error_init(error);
    apl_status reload = refresh_active(state, error);
    if (reload == APL_OK) {
        reload = refresh_teams(state, 0, error);
    }
    if (reload != APL_OK) {
        show_error_dialog(state, error);
        return;
    }
    sync_status_context(state);
    state->screen = state->any_teams ? SHELL_SCREEN_TEAM_LIST : SHELL_SCREEN_WELCOME;
    if (was_active && active_status_has_team(state->active_status)) {
        set_status_success(state, "Team archived. Active team switched to '%s'.",
                           state->active_team.name);
    } else if (was_active) {
        set_status_success(state, "Team archived. No active team remains.");
    } else {
        set_status_success(state,
                           "Team archived. Its history remains available in the archived view.");
    }
}

void perform_set_active(shell_state *const state, const int64_t team_id, apl_error *const error)
{
    const apl_status status = apl_team_service_set_active(state->app, team_id, error);
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot switch team", error);
        return;
    }
    apl_error_init(error);
    state->selected_season_id = 0;
    if (refresh_active(state, error) != APL_OK) {
        show_error_dialog(state, error);
        return;
    }
    if (state->section == APL_TUI_SECTION_MONEY) {
        state->screen = SHELL_SCREEN_MONEY_LIST;
        state->entry_detail = (apl_ledger_entry){};
        if (load_ledger(state, 0, error) != APL_OK) {
            show_error_dialog(state, error);
            return;
        }
    }
    if (state->section == APL_TUI_SECTION_EVENTS) {
        state->screen = SHELL_SCREEN_EVENT_LIST;
        apl_event_repository_free_list(state->race_events);
        state->race_events = nullptr;
        state->race_event_count = 0;
        apl_event_detail_free(&state->race_event_detail);
        apl_session_detail_free(&state->race_session_detail);
        apl_outcome_correction_free_list(state->outcome_corrections);
        state->outcome_corrections = nullptr;
        state->outcome_correction_count = 0;
        apl_tui_list_set_count(&state->list, 0);
        if (load_race_events(state, error) != APL_OK) {
            show_error_dialog(state, error);
            return;
        }
    }
    sync_status_context(state);
    set_status_success(state, "Active team: %s", state->active_team.name);
}

void toggle_archived_view(shell_state *const state, apl_error *const error)
{
    state->include_archived = !state->include_archived;
    apl_error_init(error);
    if (refresh_teams(state, 0, error) != APL_OK) {
        show_error_dialog(state, error);
        return;
    }
    set_status(state,
               state->include_archived ? "Showing archived teams." : "Hiding archived teams.");
}

void free_picker_teams(shell_state *const state)
{
    apl_team_service_free_list(state->picker_teams);
    state->picker_teams = nullptr;
    state->picker_team_count = 0;
}

void build_team_picker_dialog(shell_state *const state)
{
    apl_tui_dialog_open_choice(&state->dialog, "Switch active team",
                               "Browsing does not switch teams. Enter selects explicitly.");
    unsigned active_option = UINT_MAX;
    for (size_t index = 0; index < state->picker_team_count; ++index) {
        const apl_team *const team = &state->picker_teams[index];
        const bool active =
            active_status_has_team(state->active_status) && team->id == state->active_team.id;
        char label[APL_TUI_DIALOG_OPTION_LABEL_MAX];
        (void)snprintf(label, sizeof(label), "%s%s", team->name,
                       team->meta.archived ? " [archived]" : "");
        const unsigned option =
            apl_tui_dialog_add_option(&state->dialog, label, !team->meta.archived, active);
        if (active && option != UINT_MAX) {
            active_option = option;
        }
    }
    (void)apl_tui_dialog_add_option(&state->dialog,
                                    state->picker_include_archived ? "Hide archived teams"
                                                                   : "Show archived teams",
                                    true, false);
    (void)apl_tui_dialog_add_option(&state->dialog, "Create a new team...", true, false);
    if (active_option != UINT_MAX) {
        apl_tui_dialog_select_option(&state->dialog, active_option);
    }
    state->dialog_context = SHELL_DIALOG_CONTEXT_TEAM_PICKER;
}

apl_status load_picker_teams(shell_state *const state, apl_error *const error)
{
    free_picker_teams(state);
    return apl_team_service_list(state->app, state->picker_include_archived, &state->picker_teams,
                                 &state->picker_team_count, error);
}

void open_team_picker(shell_state *const state)
{
    apl_error error;
    apl_error_init(&error);
    state->picker_include_archived = false;
    if (load_picker_teams(state, &error) != APL_OK) {
        show_error_dialog(state, &error);
        return;
    }
    build_team_picker_dialog(state);
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Choose a team. Archived teams are shown only on request.");
}

void rebuild_team_picker(shell_state *const state, const bool include_archived)
{
    apl_error error;
    apl_error_init(&error);
    state->picker_include_archived = include_archived;
    if (load_picker_teams(state, &error) != APL_OK) {
        close_shell_dialog(state);
        show_error_dialog(state, &error);
        return;
    }
    build_team_picker_dialog(state);
}

void finish_team_picker(shell_state *const state, const apl_tui_dialog_result result)
{
    if (result == APL_TUI_DIALOG_DISMISSED) {
        close_shell_dialog(state);
        set_status(state, "Team switch cancelled.");
        return;
    }

    const unsigned selected = apl_tui_dialog_selected_option(&state->dialog);
    if (selected == UINT_MAX) {
        return;
    }
    if (selected < state->picker_team_count) {
        const apl_team team = state->picker_teams[selected];
        const bool already_active =
            active_status_has_team(state->active_status) && state->active_team.id == team.id;
        close_shell_dialog(state);
        if (team.meta.archived) {
            set_status_warning(state, "Archived teams cannot be active.");
            return;
        }
        if (already_active) {
            set_status(state, "Team '%s' is already active.", team.name);
            return;
        }
        apl_error error;
        apl_error_init(&error);
        perform_set_active(state, team.id, &error);
        if (!apl_error_is_success(&error)) {
            show_error_dialog(state, &error);
        }
        return;
    }

    const unsigned toggle_index = (unsigned)state->picker_team_count;
    if (selected == toggle_index) {
        rebuild_team_picker(state, !state->picker_include_archived);
        return;
    }
    if (selected == toggle_index + 1) {
        close_shell_dialog(state);
        open_create_form(state);
        return;
    }
}

void draw_welcome(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    apl_tui_frame_draw_line(body, 0, "Welcome to apl1.");
    apl_tui_frame_draw_line(body, 2, "No teams have been added yet.");
    apl_tui_frame_draw_line(body, 4, "Press n to create your first team.");
    apl_tui_frame_draw_line(body, 6, "Press q to quit.");
}

void draw_team_list(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }

    if (state->team_count == 0) {
        if (state->any_teams && !state->include_archived) {
            apl_tui_frame_draw_line(body, 0, "No active teams. Press a to show archived teams.");
        } else {
            apl_tui_frame_draw_line(body, 0,
                                    "No teams have been added yet. Press n to create one.");
        }
        return;
    }

    const unsigned rows = apl_tui_frame_layout(&state->frame)->body.rows;
    const size_t visible = apl_tui_list_visible_count(&state->list);
    for (size_t offset = 0; offset < visible && offset < rows; ++offset) {
        const size_t index = state->list.offset + offset;
        const apl_team *const team = &state->teams[index];
        const bool focused = index == state->list.selected;
        const bool active =
            state->active_status == APL_ACTIVE_TEAM_ACTIVE && team->id == state->active_team.id;

        char line[APL_TEAM_NAME_MAX + APL_TEAM_NOTES_MAX + 32];
        (void)snprintf(line, sizeof(line), "%c%c %s%s%s%s", focused ? '>' : ' ', active ? '*' : ' ',
                       team->name, team->meta.archived ? " [archived]" : "",
                       team->notes[0] != '\0' ? "  -  " : "", team->notes);
        apl_tui_theme_apply(&state->theme, body,
                            focused ? APL_TUI_ROLE_SELECTION : APL_TUI_ROLE_BODY);
        apl_tui_frame_draw_line(body, (int)offset, line);
        apl_tui_theme_reset(body);
    }
}

void draw_team_detail(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    const unsigned cols = apl_tui_frame_layout(&state->frame)->body.cols;

    int row = 0;
    char line[APL_TEAM_NAME_MAX + 64];
    (void)snprintf(line, sizeof(line), "Name: %s", state->detail.name);
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Status: %s",
                   state->detail.meta.archived ? "Archived" : "Active");
    apl_tui_frame_draw_line(body, row++, line);

    apl_tui_frame_draw_line(body, row++, "Notes:");
    const char *const notes = state->detail.notes[0] != '\0' ? state->detail.notes : "not recorded";
    const unsigned note_rows =
        apl_tui_frame_draw_wrapped(body, row, 2, cols > 2 ? cols - 2 : cols, notes);
    row += (note_rows > 0) ? (int)note_rows : 1;

    ++row;
    (void)snprintf(line, sizeof(line), "Created: %s", state->detail.meta.created_at);
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Updated: %s", state->detail.meta.updated_at);
    apl_tui_frame_draw_line(body, row++, line);
    if (state->detail.meta.archived) {
        (void)snprintf(line, sizeof(line), "Archived: %s", state->detail.meta.archived_at);
        apl_tui_frame_draw_line(body, row++, line);
    }

    ++row;
    apl_tui_frame_draw_line(body, row++, "Related records");
    apl_tui_frame_draw_line(body, row++, "  People: press p");
    apl_tui_frame_draw_line(body, row++, "  Seasons/periods: press o");
    apl_tui_frame_draw_line(body, row++, "  Cars/chassis: press c");
    (void)snprintf(line, sizeof(line), "  Reporting currency: %s",
                   state->detail.currency[0] != '\0' ? state->detail.currency : "not chosen");
    apl_tui_frame_draw_line(body, row++, line);
    apl_tui_frame_draw_line(body, row++, "  Money: press f (or the Money tab)");
}

void open_selected_detail(shell_state *const state)
{
    const apl_team *const team = selected_team(state);
    if (team == nullptr) {
        return;
    }
    state->detail = *team;
    state->screen = SHELL_SCREEN_TEAM_DETAIL;
    set_status(state, "Press e to edit, s to set active, x to archive, Esc to go back.");
}

void handle_list_input(shell_state *const state, const uint32_t id)
{
    apl_error error;
    apl_error_init(&error);

    switch (id) {
    case NCKEY_UP:
        apl_tui_list_move(&state->list, -1);
        return;
    case NCKEY_DOWN:
        apl_tui_list_move(&state->list, 1);
        return;
    case NCKEY_PGUP:
        apl_tui_list_page(&state->list, -1);
        return;
    case NCKEY_PGDOWN:
        apl_tui_list_page(&state->list, 1);
        return;
    case NCKEY_HOME:
        apl_tui_list_home(&state->list);
        return;
    case NCKEY_END:
        apl_tui_list_end(&state->list);
        return;
    default:
        break;
    }

    if (is_enter_key(id)) {
        open_selected_detail(state);
        return;
    }
    switch (id) {
    case 'n':
    case 'N':
        open_create_form(state);
        return;
    case 'a':
    case 'A':
        toggle_archived_view(state, &error);
        if (!apl_error_is_success(&error)) {
            show_error_dialog(state, &error);
        }
        return;
    case 's':
    case 'S':
        open_team_picker(state);
        return;
    case 'q':
    case 'Q':
        state->quit = true;
        return;
    case '[':
        switch_section(state, false);
        return;
    case ']':
        switch_section(state, true);
        return;
    default:
        return;
    }
}

void handle_detail_input(shell_state *const state, const uint32_t id)
{
    switch (id) {
    case NCKEY_ESC:
        apl_tui_list_set_count(&state->list, state->team_count);
        state->screen = state->any_teams ? SHELL_SCREEN_TEAM_LIST : SHELL_SCREEN_WELCOME;
        return;
    case 'e':
    case 'E':
        if (state->detail.meta.archived) {
            set_status_warning(state, "Archived teams cannot be edited in this phase.");
        } else {
            open_edit_form(state);
        }
        return;
    case 's':
    case 'S':
        open_team_picker(state);
        return;
    case 'p':
    case 'P':
        set_section(state, APL_TUI_SECTION_PEOPLE);
        return;
    case 'o':
    case 'O': {
        apl_error error;
        apl_error_init(&error);
        state->screen = SHELL_SCREEN_SEASON_LIST;
        if (load_seasons(state, 0, &error) != APL_OK) {
            show_error_dialog(state, &error);
        } else {
            set_status(state, "Seasons/periods. n create, a include archived, Enter open.");
        }
        return;
    }
    case 'f':
    case 'F':
    case '$': {
        set_section(state, APL_TUI_SECTION_MONEY);
        return;
    }
    case 'c':
    case 'C': {
        apl_error error;
        apl_error_init(&error);
        state->screen = SHELL_SCREEN_CAR_LIST;
        if (load_cars(state, 0, &error) != APL_OK) {
            show_error_dialog(state, &error);
        } else {
            set_status(state, "Cars/chassis. n create, a include archived, Enter open.");
        }
        return;
    }
    case 'x':
    case 'X':
        if (state->detail.meta.archived) {
            set_status_warning(state, "This team is already archived.");
        } else if (!can_archive_team(state, &state->detail)) {
            set_status(state,
                       "The last active team cannot be archived. Create another team first.");
        } else {
            state->pending = SHELL_PENDING_ARCHIVE_TEAM;
            state->pending_team_id = state->detail.id;
            apl_tui_dialog_open_confirm(&state->dialog, "Archive team?",
                                        "The team will be hidden from the active list. Its "
                                        "records and history are kept.");
            apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
        }
        return;
    case 'q':
    case 'Q':
        state->quit = true;
        return;
    case '[':
        switch_section(state, false);
        return;
    case ']':
        switch_section(state, true);
        return;
    default:
        return;
    }
}

void handle_welcome_input(shell_state *const state, const uint32_t id)
{
    switch (id) {
    case 'n':
    case 'N':
        open_create_form(state);
        return;
    case 'q':
    case 'Q':
        state->quit = true;
        return;
    case '[':
        switch_section(state, false);
        return;
    case ']':
        switch_section(state, true);
        return;
    default:
        return;
    }
}
