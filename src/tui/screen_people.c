#include "tui/shell_internal.h"

size_t find_driver_index(const shell_state *const state, const int64_t id)
{
    for (size_t index = 0; index < state->driver_count; ++index) {
        if (state->drivers[index].id == id) {
            return index;
        }
    }
    return SIZE_MAX;
}

apl_status load_people(shell_state *const state, const int64_t preferred_id, apl_error *const error)
{
    int64_t keep_id = preferred_id;
    if (keep_id == 0 && state->driver_count > 0 && state->list.selected < state->driver_count) {
        keep_id = state->drivers[state->list.selected].id;
    }
    apl_driver *drivers = nullptr;
    size_t count = 0;
    const int64_t team_id = active_team_id(state);
    apl_status status = APL_OK;
    if (state->people_all_teams) {
        status = apl_driver_service_list(state->app, state->include_archived_drivers, &drivers,
                                         &count, error);
    } else if (team_id > 0) {
        status = apl_driver_service_list_for_team(
            state->app, team_id, state->include_archived_drivers, &drivers, &count, error);
    }
    if (status != APL_OK) {
        return status;
    }
    apl_driver_service_free_list(state->drivers);
    state->drivers = drivers;
    state->driver_count = count;
    apl_tui_list_set_count(&state->list, count);
    if (keep_id != 0) {
        const size_t index = find_driver_index(state, keep_id);
        if (index != SIZE_MAX) {
            apl_tui_list_select(&state->list, index);
        }
    }
    return APL_OK;
}

apl_status load_driver_memberships(shell_state *const state, const int64_t driver_id,
                                   apl_error *const error)
{
    apl_membership *memberships = nullptr;
    size_t count = 0;
    const apl_status status =
        apl_membership_service_list_for_driver(state->app, driver_id, &memberships, &count, error);
    if (status != APL_OK) {
        return status;
    }
    apl_membership_service_free_list(state->memberships);
    state->memberships = memberships;
    state->membership_count = count;
    return APL_OK;
}

void open_driver_dialog(shell_state *const state, const bool edit, const char *const name,
                        const char *const short_name, const char *const notes)
{
    apl_tui_dialog_open_form(&state->dialog, edit ? "Edit driver" : "Create driver",
                             "Display name is required. Duplicate names stay separate.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Name", name, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Short name (optional)", short_name, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes (optional)", notes, false);
    state->dialog_context =
        edit ? SHELL_DIALOG_CONTEXT_DRIVER_EDIT : SHELL_DIALOG_CONTEXT_DRIVER_CREATE;
    state->dialog_record_id = edit ? state->driver_detail.id : 0;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter saves; Esc cancels without writing.");
}

void open_membership_add_dialog(shell_state *const state, const int64_t driver_id,
                                const char *const start_date, const char *const end_date,
                                const char *const role, const char *const notes)
{
    if (active_team_id(state) == 0) {
        set_status_warning(state, "Select an active team before adding a membership.");
        return;
    }
    apl_tui_dialog_open_form(&state->dialog, "Add team membership",
                             "Start date is required. End date and role are optional.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Start date (YYYY-MM-DD)", start_date, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "End date (YYYY-MM-DD)", end_date, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Role/seat (optional)", role, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes (optional)", notes, false);
    state->dialog_context = SHELL_DIALOG_CONTEXT_MEMBERSHIP_ADD;
    state->dialog_driver_id = driver_id;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter adds the membership; Esc writes nothing.");
}

void open_membership_end_dialog(shell_state *const state, const int64_t membership_id,
                                const char *const start_date, const char *const end_date)
{
    apl_tui_dialog_open_form(&state->dialog, "End team membership",
                             "Enter the final date; the period remains in history.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Start date (YYYY-MM-DD)", start_date, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "End date (YYYY-MM-DD)", end_date, true);
    state->dialog_context = SHELL_DIALOG_CONTEXT_MEMBERSHIP_END;
    state->dialog_record_id = membership_id;
    state->dialog.fields[0].required = false;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter ends the period; Esc writes nothing.");
}

bool perform_driver_create(shell_state *const state, const char *const name,
                           const char *const short_name, const char *const notes,
                           apl_error *const error)
{
    apl_driver created = {};
    const apl_status status =
        apl_driver_service_create(state->app, name, short_name, notes, &created, error);
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot create driver", error);
        return false;
    }
    state->driver_detail = created;
    if (load_people(state, created.id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    state->section = APL_TUI_SECTION_PEOPLE;
    state->screen = SHELL_SCREEN_PERSON_DETAIL;
    sync_status_context(state);
    set_status_success(state, "Driver '%s' created. Add a membership to link a team.",
                       created.name);
    return true;
}

bool perform_driver_update(shell_state *const state, const char *const name,
                           const char *const short_name, const char *const notes,
                           apl_error *const error)
{
    apl_driver updated = {};
    const apl_status status = apl_driver_service_update(state->app, state->dialog_record_id, name,
                                                        short_name, notes, &updated, error);
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot save driver", error);
        return false;
    }
    state->driver_detail = updated;
    if (load_people(state, updated.id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    state->screen = SHELL_SCREEN_PERSON_DETAIL;
    set_status_success(state, "Driver changes saved.");
    return true;
}

void perform_driver_archive(shell_state *const state, const int64_t id, apl_error *const error)
{
    apl_driver archived = {};
    if (apl_driver_service_archive(state->app, id, &archived, error) != APL_OK) {
        show_error_dialog(state, error);
        return;
    }
    if (load_people(state, 0, error) != APL_OK) {
        show_error_dialog(state, error);
        return;
    }
    state->screen = SHELL_SCREEN_PEOPLE_LIST;
    set_status_success(state, "Driver archived. Historical memberships remain readable; default "
                              "pickers hide the profile.");
}

bool perform_membership_add(shell_state *const state, const char *const start_date,
                            const char *const end_date, const char *const role,
                            const char *const notes, apl_error *const error)
{
    apl_membership created = {};
    const apl_status status =
        apl_membership_service_create(state->app, active_team_id(state), state->dialog_driver_id,
                                      start_date, end_date, role, notes, &created, error);
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot add membership", error);
        return false;
    }
    if (load_driver_memberships(state, state->dialog_driver_id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    if (load_people(state, state->dialog_driver_id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    state->section = APL_TUI_SECTION_PEOPLE;
    state->screen = SHELL_SCREEN_PERSON_DETAIL;
    sync_status_context(state);
    set_status_success(state, "Membership added starting %s.", created.start_date);
    return true;
}

bool perform_membership_end(shell_state *const state, const char *const end_date,
                            apl_error *const error)
{
    apl_membership ended = {};
    const apl_status status = apl_membership_service_end(
        state->app, active_team_id(state), state->dialog_record_id, end_date, &ended, error);
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot end membership", error);
        return false;
    }
    if (load_driver_memberships(state, state->driver_detail.id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    state->screen = SHELL_SCREEN_PERSON_DETAIL;
    set_status_success(state, "Membership ended on %s; the period remains in history.",
                       ended.end_date);
    return true;
}

void open_person_detail(shell_state *const state, const apl_driver driver)
{
    apl_error error;
    apl_error_init(&error);
    state->driver_detail = driver;
    if (load_driver_memberships(state, driver.id, &error) != APL_OK) {
        show_error_dialog(state, &error);
        return;
    }
    state->section = APL_TUI_SECTION_PEOPLE;
    state->screen = SHELL_SCREEN_PERSON_DETAIL;
    sync_status_context(state);
    set_status(state, "m add membership, d end current membership, e edit, x archive, Esc back.");
}

void finish_driver_picker(shell_state *const state, const apl_tui_dialog_result result)
{
    const unsigned selected = apl_tui_dialog_selected_option(&state->dialog);
    const int mode = state->driver_choice_mode;
    if (result == APL_TUI_DIALOG_DISMISSED || selected == UINT_MAX ||
        selected >= state->driver_choice_count) {
        close_shell_dialog(state);
        set_status(state, "Driver selection cancelled.");
        return;
    }
    const int64_t driver_id = state->driver_choice_ids[selected];
    close_shell_dialog(state);

    if (mode == 1) {
        open_membership_add_dialog(state, driver_id, state->pending_start, state->pending_end,
                                   state->pending_role, state->pending_notes);
        return;
    }
    if (mode == 2) {
        apl_membership *memberships = nullptr;
        size_t count = 0;
        apl_error error;
        apl_error_init(&error);
        if (apl_membership_service_list_for_driver(state->app, driver_id, &memberships, &count,
                                                   &error) != APL_OK) {
            show_error_dialog(state, &error);
            return;
        }
        int64_t membership_id = 0;
        char start_date[APL_REFERENCE_DATE_MAX] = "";
        for (size_t index = count; index > 0; --index) {
            if (memberships[index - 1].end_date[0] == '\0') {
                membership_id = memberships[index - 1].id;
                (void)snprintf(start_date, sizeof(start_date), "%s",
                               memberships[index - 1].start_date);
                break;
            }
        }
        apl_membership_service_free_list(memberships);
        if (membership_id == 0) {
            set_status_warning(state, "That driver has no current membership period to end.");
            return;
        }
        open_membership_end_dialog(state, membership_id, start_date, state->pending_end);
        return;
    }

    apl_driver driver = {};
    apl_error error;
    apl_error_init(&error);
    if (apl_driver_service_get(state->app, driver_id, &driver, &error) != APL_OK) {
        show_error_dialog(state, &error);
        return;
    }
    open_person_detail(state, driver);
}

void open_driver_choice(shell_state *const state, apl_driver *const drivers, const size_t count,
                        const int mode)
{
    const char *const title = (mode == 1)   ? "Add membership for driver"
                              : (mode == 2) ? "End membership for driver"
                                            : "Choose driver";
    apl_tui_dialog_open_choice(&state->dialog, title,
                               "Duplicate names remain separate records. Choose explicitly by ID.");
    state->driver_choice_count = 0;
    state->driver_choice_mode = mode;
    for (size_t index = 0; index < count && index < APL_TUI_DIALOG_OPTIONS_MAX; ++index) {
        char label[APL_TUI_DIALOG_OPTION_LABEL_MAX];
        if (drivers[index].short_name[0] != '\0') {
            (void)snprintf(label, sizeof(label), "%.80s (%.40s)  (#%lld)", drivers[index].name,
                           drivers[index].short_name, (long long)drivers[index].id);
        } else {
            (void)snprintf(label, sizeof(label), "%.80s  (#%lld)", drivers[index].name,
                           (long long)drivers[index].id);
        }
        const unsigned option =
            apl_tui_dialog_add_option(&state->dialog, label, !drivers[index].meta.archived, false);
        if (option != UINT_MAX) {
            state->driver_choice_ids[state->driver_choice_count++] = drivers[index].id;
        }
    }
    state->dialog_context = SHELL_DIALOG_CONTEXT_DRIVER_PICKER;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Up/Down selects; Enter continues; Esc cancels.");
}

void draw_people_list(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    if (!state->people_all_teams && active_team_id(state) == 0) {
        apl_tui_frame_draw_line(
            body, 0, "No active team. Select one, or press A for an explicit all-teams view.");
        return;
    }
    if (state->driver_count == 0) {
        const char *const scope = state->people_all_teams ? "all teams" : "this team";
        char line[160];
        (void)snprintf(line, sizeof(line),
                       "No drivers for %s. Press n to create a driver profile%s.", scope,
                       state->people_all_teams ? "" : ", then m to add a membership");
        apl_tui_frame_draw_line(body, 0, line);
        return;
    }
    const unsigned rows = apl_tui_frame_layout(&state->frame)->body.rows;
    const size_t visible = apl_tui_list_visible_count(&state->list);
    for (size_t offset = 0; offset < visible && offset < rows; ++offset) {
        const size_t index = state->list.offset + offset;
        const apl_driver *const driver = &state->drivers[index];
        const bool focused = index == state->list.selected;
        apl_membership *memberships = nullptr;
        size_t membership_count = 0;
        apl_error error;
        apl_error_init(&error);
        bool has_current = false;
        if (apl_membership_service_list_for_driver(state->app, driver->id, &memberships,
                                                   &membership_count, &error) == APL_OK) {
            for (size_t member = 0; member < membership_count; ++member) {
                if (memberships[member].end_date[0] == '\0') {
                    has_current = true;
                    break;
                }
            }
            apl_membership_service_free_list(memberships);
        }
        const char *const membership_state =
            has_current
                ? "current membership"
                : (membership_count > 0 ? "historical memberships only" : "no team membership");
        char line[APL_REFERENCE_LABEL_MAX + APL_REFERENCE_SHORT_MAX + 96];
        if (driver->short_name[0] != '\0') {
            (void)snprintf(line, sizeof(line), "%c %.80s (%.40s)  [%s]%s", focused ? '>' : ' ',
                           driver->name, driver->short_name, membership_state,
                           driver->meta.archived ? " [archived]" : "");
        } else {
            (void)snprintf(line, sizeof(line), "%c %.100s  [%s]%s", focused ? '>' : ' ',
                           driver->name, membership_state,
                           driver->meta.archived ? " [archived]" : "");
        }
        apl_tui_theme_apply(&state->theme, body,
                            focused ? APL_TUI_ROLE_SELECTION : APL_TUI_ROLE_BODY);
        apl_tui_frame_draw_line(body, (int)offset, line);
        apl_tui_theme_reset(body);
    }
}

void draw_person_detail(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    const unsigned cols = apl_tui_frame_layout(&state->frame)->body.cols;
    const apl_driver *const driver = &state->driver_detail;
    int row = 0;
    char line[APL_REFERENCE_LABEL_MAX + 96];
    (void)snprintf(line, sizeof(line), "Name: %s", driver->name);
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Short name: %s",
                   driver->short_name[0] != '\0' ? driver->short_name : "not recorded");
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Status: %s", driver->meta.archived ? "Archived" : "Active");
    apl_tui_frame_draw_line(body, row++, line);
    apl_tui_frame_draw_line(body, row++, "Notes:");
    const unsigned note_rows =
        apl_tui_frame_draw_wrapped(body, row, 2, cols > 2 ? cols - 2 : cols,
                                   driver->notes[0] != '\0' ? driver->notes : "not recorded");
    row += (int)(note_rows > 0 ? note_rows : 1);
    ++row;

    apl_tui_frame_draw_line(body, row++, "Team membership periods (newest start first):");
    if (state->membership_count == 0) {
        apl_tui_frame_draw_line(body, row++, "  No membership recorded. Press m to add one.");
    } else {
        for (size_t index = state->membership_count; index > 0; --index) {
            const apl_membership *const membership = &state->memberships[index - 1];
            if (row >= (int)apl_tui_frame_layout(&state->frame)->body.rows) {
                break;
            }
            apl_team team = {};
            apl_error error;
            apl_error_init(&error);
            const char *team_name = "(unknown team)";
            if (apl_team_service_get(state->app, membership->team_id, &team, &error) == APL_OK) {
                team_name = team.name;
            }
            (void)snprintf(
                line, sizeof(line), "  %s  %s to %s%s%s  #%lld", team_name, membership->start_date,
                membership->end_date[0] != '\0' ? membership->end_date : "open",
                membership->role[0] != '\0' ? "  role: " : "",
                membership->role[0] != '\0' ? membership->role : "", (long long)membership->id);
            apl_tui_frame_draw_line(body, row++, line);
        }
    }
    ++row;
    (void)snprintf(line, sizeof(line), "Created: %s", driver->meta.created_at);
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Updated: %s", driver->meta.updated_at);
    apl_tui_frame_draw_line(body, row++, line);
    ++row;
    apl_tui_frame_draw_line(body, row++, "m add membership   d end current membership");
    apl_tui_frame_draw_line(body, row++, "e edit driver   x archive   Esc back to people");
}

void open_selected_person(shell_state *const state)
{
    if (state->driver_count == 0 || state->list.selected >= state->driver_count) {
        return;
    }
    open_person_detail(state, state->drivers[state->list.selected]);
}

void handle_people_list_input(shell_state *const state, const uint32_t id)
{
    apl_error error;
    apl_error_init(&error);
    if (handle_body_list_navigation(state, id)) {
        return;
    }
    switch (id) {
    case NCKEY_ESC:
        set_section(state, APL_TUI_SECTION_TEAMS);
        return;
    case 'n':
    case 'N':
        open_driver_dialog(state, false, "", "", "");
        return;
    case 'a':
    case 'A':
        if (id == 'A') {
            state->people_all_teams = !state->people_all_teams;
            if (load_people(state, 0, &error) != APL_OK) {
                show_error_dialog(state, &error);
                return;
            }
            set_status(state, state->people_all_teams
                                  ? "Showing the explicit all-teams driver directory."
                                  : "Showing drivers associated with the active team.");
        } else {
            state->include_archived_drivers = !state->include_archived_drivers;
            if (load_people(state, 0, &error) != APL_OK) {
                show_error_dialog(state, &error);
                return;
            }
            set_status(state, state->include_archived_drivers ? "Including archived drivers."
                                                              : "Hiding archived drivers.");
        }
        return;
    case 'm':
    case 'M':
        if (state->driver_count > 0 && state->list.selected < state->driver_count) {
            open_membership_add_dialog(state, state->drivers[state->list.selected].id, "", "", "",
                                       "");
        }
        return;
    case 'q':
    case 'Q':
        state->quit = true;
        return;
    default:
        break;
    }
    if (is_enter_key(id)) {
        open_selected_person(state);
    }
}

void handle_person_detail_input(shell_state *const state, const uint32_t id)
{
    apl_error error;
    apl_error_init(&error);
    switch (id) {
    case NCKEY_ESC:
        if (load_people(state, state->driver_detail.id, &error) != APL_OK) {
            show_error_dialog(state, &error);
            return;
        }
        state->screen = SHELL_SCREEN_PEOPLE_LIST;
        return;
    case 'm':
    case 'M':
        open_membership_add_dialog(state, state->driver_detail.id, "", "", "", "");
        return;
    case 'd':
    case 'D': {
        int64_t membership_id = 0;
        char start_date[APL_REFERENCE_DATE_MAX] = "";
        char end_date[APL_REFERENCE_DATE_MAX] = "";
        for (size_t index = state->membership_count; index > 0; --index) {
            if (state->memberships[index - 1].end_date[0] == '\0') {
                membership_id = state->memberships[index - 1].id;
                (void)snprintf(start_date, sizeof(start_date), "%s",
                               state->memberships[index - 1].start_date);
                (void)snprintf(end_date, sizeof(end_date), "%s",
                               state->memberships[index - 1].end_date);
                break;
            }
        }
        if (membership_id == 0) {
            set_status_warning(state, "No current membership period can be ended.");
        } else {
            open_membership_end_dialog(state, membership_id, start_date, end_date);
        }
        return;
    }
    case 'e':
    case 'E':
        if (state->driver_detail.meta.archived) {
            set_status_warning(state, "Archived drivers are read-only in this view.");
        } else {
            open_driver_dialog(state, true, state->driver_detail.name,
                               state->driver_detail.short_name, state->driver_detail.notes);
        }
        return;
    case 'x':
    case 'X':
        if (state->driver_detail.meta.archived) {
            set_status_warning(state, "This driver is already archived.");
        } else {
            state->pending = SHELL_PENDING_ARCHIVE_DRIVER;
            state->pending_record_id = state->driver_detail.id;
            apl_tui_dialog_open_confirm(&state->dialog, "Archive driver?",
                                        "Historical memberships remain readable. The driver is "
                                        "hidden from default current pickers.");
            apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
        }
        return;
    case 'q':
    case 'Q':
        state->quit = true;
        return;
    default:
        return;
    }
}
