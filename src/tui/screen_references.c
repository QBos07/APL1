#include "tui/shell_internal.h"

const char *selected_season_label(const shell_state *const state)
{
    if (state->selected_season_id == 0) {
        return nullptr;
    }
    if (state->season_detail.id == state->selected_season_id) {
        return state->season_detail.label;
    }
    for (size_t index = 0; index < state->season_count; ++index) {
        if (state->seasons[index].id == state->selected_season_id) {
            return state->seasons[index].label;
        }
    }
    return nullptr;
}

size_t find_season_index(const shell_state *const state, const int64_t id)
{
    for (size_t index = 0; index < state->season_count; ++index) {
        if (state->seasons[index].id == id) {
            return index;
        }
    }
    return SIZE_MAX;
}

size_t find_car_index(const shell_state *const state, const int64_t id)
{
    for (size_t index = 0; index < state->car_count; ++index) {
        if (state->cars[index].id == id) {
            return index;
        }
    }
    return SIZE_MAX;
}

apl_status load_seasons(shell_state *const state, const int64_t preferred_id,
                        apl_error *const error)
{
    int64_t keep_id = preferred_id;
    if (keep_id == 0 && state->season_count > 0 && state->list.selected < state->season_count) {
        keep_id = state->seasons[state->list.selected].id;
    }
    apl_season *seasons = nullptr;
    size_t count = 0;
    const int64_t team_id = active_team_id(state);
    apl_status status = APL_OK;
    if (team_id > 0) {
        status = apl_season_service_list(state->app, team_id, state->include_archived_seasons,
                                         &seasons, &count, error);
    }
    if (status != APL_OK) {
        return status;
    }
    apl_season_service_free_list(state->seasons);
    state->seasons = seasons;
    state->season_count = count;
    apl_tui_list_set_count(&state->list, count);
    if (keep_id != 0) {
        const size_t index = find_season_index(state, keep_id);
        if (index != SIZE_MAX) {
            apl_tui_list_select(&state->list, index);
        }
    }
    return APL_OK;
}

apl_status load_cars(shell_state *const state, const int64_t preferred_id, apl_error *const error)
{
    int64_t keep_id = preferred_id;
    if (keep_id == 0 && state->car_count > 0 && state->list.selected < state->car_count) {
        keep_id = state->cars[state->list.selected].id;
    }
    apl_car *cars = nullptr;
    size_t count = 0;
    const int64_t team_id = active_team_id(state);
    apl_status status = APL_OK;
    if (team_id > 0) {
        status = apl_car_service_list(state->app, team_id, state->include_archived_cars, &cars,
                                      &count, error);
    }
    if (status != APL_OK) {
        return status;
    }
    apl_car_service_free_list(state->cars);
    state->cars = cars;
    state->car_count = count;
    apl_tui_list_set_count(&state->list, count);
    if (keep_id != 0) {
        const size_t index = find_car_index(state, keep_id);
        if (index != SIZE_MAX) {
            apl_tui_list_select(&state->list, index);
        }
    }
    return APL_OK;
}

void open_season_dialog(shell_state *const state, const bool edit, const char *const label,
                        const char *const start_date, const char *const end_date,
                        const char *const notes)
{
    apl_tui_dialog_open_form(&state->dialog, edit ? "Edit season/period" : "Create season/period",
                             "Label is required. Dates use YYYY-MM-DD or stay blank.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Label", label, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Start date (YYYY-MM-DD)", start_date, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "End date (YYYY-MM-DD)", end_date, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes (optional)", notes, false);
    state->dialog_context =
        edit ? SHELL_DIALOG_CONTEXT_SEASON_EDIT : SHELL_DIALOG_CONTEXT_SEASON_CREATE;
    state->dialog_record_id = edit ? state->season_detail.id : 0;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter saves; Esc cancels without writing.");
}

void open_car_dialog(shell_state *const state, const bool edit, const char *const label,
                     const char *const chassis, const char *const notes)
{
    apl_tui_dialog_open_form(&state->dialog, edit ? "Edit car/chassis" : "Create car/chassis",
                             "Identifier is required. Chassis and notes are optional.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Identifier", label, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Chassis (optional)", chassis, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes (optional)", notes, false);
    state->dialog_context = edit ? SHELL_DIALOG_CONTEXT_CAR_EDIT : SHELL_DIALOG_CONTEXT_CAR_CREATE;
    state->dialog_record_id = edit ? state->car_detail.id : 0;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter saves; Esc cancels without writing.");
}

bool perform_season_create(shell_state *const state, const char *const label,
                           const char *const start_date, const char *const end_date,
                           const char *const notes, apl_error *const error)
{
    apl_season created = {};
    const apl_status status = apl_season_service_create(
        state->app, active_team_id(state), label, start_date, end_date, notes, &created, error);
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot create season", error);
        return false;
    }
    state->season_detail = created;
    if (load_seasons(state, created.id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    state->section = APL_TUI_SECTION_TEAMS;
    state->screen = SHELL_SCREEN_SEASON_DETAIL;
    sync_status_context(state);
    set_status_success(state, "Season '%s' created.", created.label);
    return true;
}

bool perform_season_update(shell_state *const state, const char *const label,
                           const char *const start_date, const char *const end_date,
                           const char *const notes, apl_error *const error)
{
    apl_season updated = {};
    const apl_status status =
        apl_season_service_update(state->app, active_team_id(state), state->dialog_record_id, label,
                                  start_date, end_date, notes, &updated, error);
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot save season", error);
        return false;
    }
    state->season_detail = updated;
    if (load_seasons(state, updated.id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    state->screen = SHELL_SCREEN_SEASON_DETAIL;
    set_status_success(state, "Season changes saved.");
    return true;
}

void perform_season_archive(shell_state *const state, const int64_t id, apl_error *const error)
{
    apl_season archived = {};
    if (apl_season_service_archive(state->app, active_team_id(state), id, &archived, error) !=
        APL_OK) {
        show_error_dialog(state, error);
        return;
    }
    if (state->selected_season_id == id) {
        state->selected_season_id = 0;
        sync_status_context(state);
    }
    if (load_seasons(state, 0, error) != APL_OK) {
        show_error_dialog(state, error);
        return;
    }
    state->screen = SHELL_SCREEN_SEASON_LIST;
    set_status_success(state, "Season archived. It remains available in history.");
}

bool perform_car_create(shell_state *const state, const char *const label,
                        const char *const chassis, const char *const notes, apl_error *const error)
{
    apl_car created = {};
    const apl_status status = apl_car_service_create(state->app, active_team_id(state), label,
                                                     chassis, notes, &created, error);
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot create car", error);
        return false;
    }
    state->car_detail = created;
    if (load_cars(state, created.id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    state->section = APL_TUI_SECTION_TEAMS;
    state->screen = SHELL_SCREEN_CAR_DETAIL;
    sync_status_context(state);
    set_status_success(state, "Car '%s' created.", created.label);
    return true;
}

bool perform_car_update(shell_state *const state, const char *const label,
                        const char *const chassis, const char *const notes, apl_error *const error)
{
    apl_car updated = {};
    const apl_status status =
        apl_car_service_update(state->app, active_team_id(state), state->dialog_record_id, label,
                               chassis, notes, &updated, error);
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot save car", error);
        return false;
    }
    state->car_detail = updated;
    if (load_cars(state, updated.id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    state->screen = SHELL_SCREEN_CAR_DETAIL;
    set_status_success(state, "Car changes saved.");
    return true;
}

void perform_car_archive(shell_state *const state, const int64_t id, apl_error *const error)
{
    apl_car archived = {};
    if (apl_car_service_archive(state->app, active_team_id(state), id, &archived, error) !=
        APL_OK) {
        show_error_dialog(state, error);
        return;
    }
    if (load_cars(state, 0, error) != APL_OK) {
        show_error_dialog(state, error);
        return;
    }
    state->screen = SHELL_SCREEN_CAR_LIST;
    set_status_success(state, "Car archived. It remains available for reference/history.");
}

void draw_season_list(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    if (active_team_id(state) == 0) {
        apl_tui_frame_draw_line(body, 0, "No active team. Select a team before browsing seasons.");
        return;
    }
    if (state->season_count == 0) {
        apl_tui_frame_draw_line(body, 0,
                                state->include_archived_seasons
                                    ? "No seasons or periods recorded. Press n to create one."
                                    : "No current seasons. Press n to create one, or a to include "
                                      "archived periods.");
        return;
    }
    const unsigned rows = apl_tui_frame_layout(&state->frame)->body.rows;
    const size_t visible = apl_tui_list_visible_count(&state->list);
    for (size_t offset = 0; offset < visible && offset < rows; ++offset) {
        const size_t index = state->list.offset + offset;
        const apl_season *const season = &state->seasons[index];
        const bool focused = index == state->list.selected;
        const bool selected = season->id == state->selected_season_id;
        char dates[32] = "";
        if (season->start_date[0] != '\0' || season->end_date[0] != '\0') {
            (void)snprintf(dates, sizeof(dates), " [%s..%s]",
                           season->start_date[0] != '\0' ? season->start_date : "?",
                           season->end_date[0] != '\0' ? season->end_date : "open");
        }
        char line[APL_REFERENCE_LABEL_MAX + APL_REFERENCE_NOTES_MAX + 96];
        (void)snprintf(line, sizeof(line), "%c%c %s%s%s%s%.200s", focused ? '>' : ' ',
                       selected ? '*' : ' ', season->label, dates,
                       season->meta.archived ? " [archived]" : "",
                       season->notes[0] != '\0' ? "  -  " : "", season->notes);
        apl_tui_theme_apply(&state->theme, body,
                            focused ? APL_TUI_ROLE_SELECTION : APL_TUI_ROLE_BODY);
        apl_tui_frame_draw_line(body, (int)offset, line);
        apl_tui_theme_reset(body);
    }
}

void draw_season_detail(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    const unsigned cols = apl_tui_frame_layout(&state->frame)->body.cols;
    const apl_season *const season = &state->season_detail;
    int row = 0;
    char line[APL_REFERENCE_LABEL_MAX + 96];
    (void)snprintf(line, sizeof(line), "Label: %s", season->label);
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Status: %s", season->meta.archived ? "Archived" : "Active");
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Start date: %s",
                   season->start_date[0] != '\0' ? season->start_date : "not recorded");
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "End date: %s",
                   season->end_date[0] != '\0' ? season->end_date : "not recorded");
    apl_tui_frame_draw_line(body, row++, line);
    apl_tui_frame_draw_line(body, row++, "Notes:");
    const unsigned note_rows =
        apl_tui_frame_draw_wrapped(body, row, 2, cols > 2 ? cols - 2 : cols,
                                   season->notes[0] != '\0' ? season->notes : "not recorded");
    row += (int)(note_rows > 0 ? note_rows : 1);
    ++row;
    (void)snprintf(line, sizeof(line), "Created: %s", season->meta.created_at);
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Updated: %s", season->meta.updated_at);
    apl_tui_frame_draw_line(body, row++, line);
    if (season->meta.archived) {
        (void)snprintf(line, sizeof(line), "Archived: %s", season->meta.archived_at);
        apl_tui_frame_draw_line(body, row++, line);
    }
    ++row;
    apl_tui_frame_draw_line(body, row++, "e edit   x archive   s select as current view context");
    apl_tui_frame_draw_line(body, row++, "Esc back to seasons");
}

void draw_car_list(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    if (active_team_id(state) == 0) {
        apl_tui_frame_draw_line(body, 0, "No active team. Select a team before browsing cars.");
        return;
    }
    if (state->car_count == 0) {
        apl_tui_frame_draw_line(body, 0,
                                state->include_archived_cars
                                    ? "No cars/chassis recorded. Press n to create one."
                                    : "No current cars. Press n to create one, or a to include "
                                      "archived cars.");
        return;
    }
    const unsigned rows = apl_tui_frame_layout(&state->frame)->body.rows;
    const size_t visible = apl_tui_list_visible_count(&state->list);
    for (size_t offset = 0; offset < visible && offset < rows; ++offset) {
        const size_t index = state->list.offset + offset;
        const apl_car *const car = &state->cars[index];
        const bool focused = index == state->list.selected;
        char line[APL_REFERENCE_LABEL_MAX + APL_REFERENCE_SHORT_MAX + 32];
        (void)snprintf(line, sizeof(line), "%c %s%s%s%s", focused ? '>' : ' ', car->label,
                       car->chassis[0] != '\0' ? "  chassis: " : "",
                       car->chassis[0] != '\0' ? car->chassis : "",
                       car->meta.archived ? " [archived]" : "");
        apl_tui_theme_apply(&state->theme, body,
                            focused ? APL_TUI_ROLE_SELECTION : APL_TUI_ROLE_BODY);
        apl_tui_frame_draw_line(body, (int)offset, line);
        apl_tui_theme_reset(body);
    }
}

void draw_car_detail(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    const unsigned cols = apl_tui_frame_layout(&state->frame)->body.cols;
    const apl_car *const car = &state->car_detail;
    int row = 0;
    char line[APL_REFERENCE_LABEL_MAX + APL_REFERENCE_SHORT_MAX + 32];
    (void)snprintf(line, sizeof(line), "Identifier: %s", car->label);
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Chassis: %s",
                   car->chassis[0] != '\0' ? car->chassis : "not recorded");
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Status: %s", car->meta.archived ? "Archived" : "Active");
    apl_tui_frame_draw_line(body, row++, line);
    apl_tui_frame_draw_line(body, row++, "Notes:");
    const unsigned note_rows =
        apl_tui_frame_draw_wrapped(body, row, 2, cols > 2 ? cols - 2 : cols,
                                   car->notes[0] != '\0' ? car->notes : "not recorded");
    row += (int)(note_rows > 0 ? note_rows : 1);
    ++row;
    (void)snprintf(line, sizeof(line), "Created: %s", car->meta.created_at);
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Updated: %s", car->meta.updated_at);
    apl_tui_frame_draw_line(body, row++, line);
    if (car->meta.archived) {
        (void)snprintf(line, sizeof(line), "Archived: %s", car->meta.archived_at);
        apl_tui_frame_draw_line(body, row++, line);
    }
    ++row;
    apl_tui_frame_draw_line(body, row++, "e edit   x archive   Esc back to cars");
}

void open_selected_season(shell_state *const state)
{
    if (state->season_count == 0 || state->list.selected >= state->season_count) {
        return;
    }
    const apl_season selected = state->seasons[state->list.selected];
    apl_error error;
    apl_error_init(&error);
    if (apl_season_service_get(state->app, active_team_id(state), selected.id,
                               &state->season_detail, &error) != APL_OK) {
        show_error_dialog(state, &error);
        return;
    }
    state->screen = SHELL_SCREEN_SEASON_DETAIL;
    set_status(state, "e edit, x archive, s select as current view context, Esc back.");
}

void open_selected_car(shell_state *const state)
{
    if (state->car_count == 0 || state->list.selected >= state->car_count) {
        return;
    }
    const apl_car selected = state->cars[state->list.selected];
    apl_error error;
    apl_error_init(&error);
    if (apl_car_service_get(state->app, active_team_id(state), selected.id, &state->car_detail,
                            &error) != APL_OK) {
        show_error_dialog(state, &error);
        return;
    }
    state->screen = SHELL_SCREEN_CAR_DETAIL;
    set_status(state, "e edit, x archive, Esc back.");
}

void handle_season_list_input(shell_state *const state, const uint32_t id)
{
    apl_error error;
    apl_error_init(&error);
    if (handle_body_list_navigation(state, id)) {
        return;
    }
    switch (id) {
    case NCKEY_ESC:
        state->screen = SHELL_SCREEN_TEAM_DETAIL;
        return;
    case 'n':
    case 'N':
        if (active_team_id(state) == 0) {
            set_status_warning(state, "Select an active team before creating a season.");
        } else {
            open_season_dialog(state, false, "", "", "", "");
        }
        return;
    case 'a':
    case 'A':
        state->include_archived_seasons = !state->include_archived_seasons;
        if (load_seasons(state, 0, &error) != APL_OK) {
            show_error_dialog(state, &error);
            return;
        }
        set_status(state, state->include_archived_seasons ? "Including archived seasons."
                                                          : "Hiding archived seasons.");
        return;
    case 'q':
    case 'Q':
        state->quit = true;
        return;
    default:
        break;
    }
    if (is_enter_key(id)) {
        open_selected_season(state);
    }
}

void handle_season_detail_input(shell_state *const state, const uint32_t id)
{
    switch (id) {
    case NCKEY_ESC: {
        apl_error error;
        apl_error_init(&error);
        if (load_seasons(state, state->season_detail.id, &error) != APL_OK) {
            show_error_dialog(state, &error);
            return;
        }
        state->screen = SHELL_SCREEN_SEASON_LIST;
        return;
    }
    case 'e':
    case 'E':
        if (state->season_detail.meta.archived) {
            set_status_warning(state, "Archived seasons are read-only in this view.");
        } else {
            open_season_dialog(state, true, state->season_detail.label,
                               state->season_detail.start_date, state->season_detail.end_date,
                               state->season_detail.notes);
        }
        return;
    case 'x':
    case 'X':
        if (state->season_detail.meta.archived) {
            set_status_warning(state, "This season is already archived.");
        } else {
            state->pending = SHELL_PENDING_ARCHIVE_SEASON;
            state->pending_record_id = state->season_detail.id;
            apl_tui_dialog_open_confirm(&state->dialog, "Archive season?",
                                        "The season remains readable in archived/historical views. "
                                        "No records are deleted.");
            apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
        }
        return;
    case 's':
    case 'S':
        state->selected_season_id = state->season_detail.id;
        sync_status_context(state);
        set_status_success(state,
                           "Selected season '%s' for the active team; view context only, no "
                           "records changed.",
                           state->season_detail.label);
        return;
    case 'q':
    case 'Q':
        state->quit = true;
        return;
    default:
        return;
    }
}

void handle_car_list_input(shell_state *const state, const uint32_t id)
{
    apl_error error;
    apl_error_init(&error);
    if (handle_body_list_navigation(state, id)) {
        return;
    }
    switch (id) {
    case NCKEY_ESC:
        state->screen = SHELL_SCREEN_TEAM_DETAIL;
        return;
    case 'n':
    case 'N':
        if (active_team_id(state) == 0) {
            set_status_warning(state, "Select an active team before creating a car.");
        } else {
            open_car_dialog(state, false, "", "", "");
        }
        return;
    case 'a':
    case 'A':
        state->include_archived_cars = !state->include_archived_cars;
        if (load_cars(state, 0, &error) != APL_OK) {
            show_error_dialog(state, &error);
            return;
        }
        set_status(state, state->include_archived_cars ? "Including archived cars."
                                                       : "Hiding archived cars.");
        return;
    case 'q':
    case 'Q':
        state->quit = true;
        return;
    default:
        break;
    }
    if (is_enter_key(id)) {
        open_selected_car(state);
    }
}

void handle_car_detail_input(shell_state *const state, const uint32_t id)
{
    switch (id) {
    case NCKEY_ESC: {
        apl_error error;
        apl_error_init(&error);
        if (load_cars(state, state->car_detail.id, &error) != APL_OK) {
            show_error_dialog(state, &error);
            return;
        }
        state->screen = SHELL_SCREEN_CAR_LIST;
        return;
    }
    case 'e':
    case 'E':
        if (state->car_detail.meta.archived) {
            set_status_warning(state, "Archived cars are read-only in this view.");
        } else {
            open_car_dialog(state, true, state->car_detail.label, state->car_detail.chassis,
                            state->car_detail.notes);
        }
        return;
    case 'x':
    case 'X':
        if (state->car_detail.meta.archived) {
            set_status_warning(state, "This car is already archived.");
        } else {
            state->pending = SHELL_PENDING_ARCHIVE_CAR;
            state->pending_record_id = state->car_detail.id;
            apl_tui_dialog_open_confirm(&state->dialog, "Archive car?",
                                        "The car remains available to historical references. "
                                        "No part or performance data is created.");
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
