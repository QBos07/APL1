#include "tui/shell_internal.h"

#include <stdio.h>
#include <string.h>

static void copy_text(char *const out, const size_t out_size, const char *const text)
{
    (void)snprintf(out, out_size, "%s", (text != nullptr) ? text : "");
}

static void season_label(const shell_state *const state, const int64_t season_id, char *const out,
                         const size_t out_size)
{
    out[0] = '\0';
    if (season_id <= 0) {
        return;
    }
    apl_season season = {};
    apl_error error;
    apl_error_init(&error);
    if (apl_season_service_get(state->app, active_team_id(state), season_id, &season, &error) ==
        APL_OK) {
        copy_text(out, out_size, season.label);
    } else {
        copy_text(out, out_size, "(unknown season)");
    }
}

static void car_label(const shell_state *const state, const int64_t car_id, char *const out,
                      const size_t out_size)
{
    out[0] = '\0';
    if (car_id <= 0) {
        return;
    }
    apl_car car = {};
    apl_error error;
    apl_error_init(&error);
    if (apl_car_service_get(state->app, active_team_id(state), car_id, &car, &error) == APL_OK) {
        copy_text(out, out_size, car.label);
    } else {
        copy_text(out, out_size, "(unknown car)");
    }
}

size_t find_project_index(const shell_state *const state, const int64_t id)
{
    for (size_t index = 0; index < state->project_count; ++index) {
        if (state->projects[index].id == id) {
            return index;
        }
    }
    return SIZE_MAX;
}

apl_status load_projects(shell_state *const state, const int64_t preferred_id,
                         apl_error *const error)
{
    int64_t keep_id = preferred_id;
    if (keep_id == 0 && state->project_count > 0 && state->list.selected < state->project_count) {
        keep_id = state->projects[state->list.selected].id;
    }
    apl_project *projects = nullptr;
    size_t count = 0;
    const int64_t team_id = active_team_id(state);
    apl_status status = APL_OK;
    if (team_id > 0) {
        apl_project_filter filter = {.team_id = team_id,
                                     .include_archived = state->include_archived_projects,
                                     .season_id = state->project_season_filter};
        copy_text(filter.status_key, sizeof(filter.status_key), state->project_status_filter);
        copy_text(filter.category_key, sizeof(filter.category_key), state->project_category_filter);
        status = apl_project_service_list(state->app, &filter, &projects, &count, error);
    }
    if (status != APL_OK) {
        return status;
    }
    apl_project_service_free_list(state->projects);
    state->projects = projects;
    state->project_count = count;
    apl_tui_list_set_count(&state->list, count);
    if (keep_id != 0) {
        const size_t index = find_project_index(state, keep_id);
        if (index != SIZE_MAX) {
            apl_tui_list_select(&state->list, index);
        }
    }
    return APL_OK;
}

apl_status load_project_detail(shell_state *const state, const int64_t project_id,
                               apl_error *const error)
{
    const int64_t team_id = active_team_id(state);
    if (team_id <= 0 || project_id <= 0) {
        return APL_OK;
    }
    apl_project project = {};
    apl_status status = apl_project_service_get(state->app, team_id, project_id, &project, error);
    if (status != APL_OK) {
        return status;
    }
    apl_project_update *updates = nullptr;
    size_t update_count = 0;
    status = apl_project_service_list_updates(state->app, team_id, project_id, &updates,
                                              &update_count, error);
    if (status != APL_OK) {
        return status;
    }
    apl_ledger_entry *entries = nullptr;
    size_t entry_count = 0;
    status = apl_ledger_service_list_for_project(state->app, team_id, project_id, &entries,
                                                 &entry_count, error);
    if (status != APL_OK) {
        apl_project_service_free_updates(updates);
        return status;
    }
    apl_project_cost cost = {};
    status = apl_ledger_service_project_cost(state->app, team_id, project_id, &cost, error);
    if (status != APL_OK) {
        apl_ledger_service_free_list(entries);
        apl_project_service_free_updates(updates);
        return status;
    }

    state->project_detail = project;
    apl_project_service_free_updates(state->project_updates);
    state->project_updates = updates;
    state->project_update_count = update_count;
    apl_ledger_service_free_list(state->project_entries);
    state->project_entries = entries;
    state->project_entry_count = entry_count;
    state->project_cost = cost;
    apl_tui_list_set_count(&state->list, entry_count);
    return APL_OK;
}

void open_project_detail(shell_state *const state, const apl_project project)
{
    apl_error error;
    apl_error_init(&error);
    state->project_detail = project;
    if (load_project_detail(state, project.id, &error) != APL_OK) {
        show_error_dialog(state, &error);
        return;
    }
    state->section = APL_TUI_SECTION_DEVELOPMENT;
    state->screen = SHELL_SCREEN_PROJECT_DETAIL;
    sync_status_context(state);
    set_status(state, "u log update | e edit | x archive | Enter linked cost | Esc projects.");
}

void open_selected_project(shell_state *const state)
{
    if (state->project_count == 0 || state->list.selected >= state->project_count) {
        return;
    }
    open_project_detail(state, state->projects[state->list.selected]);
}

static void project_filter_status_text(const shell_state *const state, char *const out,
                                       const size_t out_size)
{
    if (state->project_status_filter[0] == '\0') {
        copy_text(out, out_size, "any");
    } else {
        copy_text(out, out_size, state->project_status_filter);
    }
}

void build_project_filter_bar(const shell_state *const state, char *const out,
                              const size_t out_size, unsigned starts[5], unsigned ends[5])
{
    out[0] = '\0';
    unsigned cursor = 0;
    char label[160];
    char status[APL_PROJECT_STATUS_MAX];
    project_filter_status_text(state, status, sizeof(status));
    (void)snprintf(label, sizeof(label), "Status:%s", status);
    append_filter_button(out, out_size, &cursor, starts, ends, 0, label);

    if (state->project_season_filter <= 0) {
        (void)snprintf(label, sizeof(label), "Season:any");
    } else {
        char season[APL_REFERENCE_LABEL_MAX];
        season_label(state, state->project_season_filter, season, sizeof(season));
        (void)snprintf(label, sizeof(label), "Season:%.24s", season);
    }
    append_filter_button(out, out_size, &cursor, starts, ends, 1, label);

    (void)snprintf(label, sizeof(label), "Category:%s",
                   state->project_category_filter[0] != '\0' ? state->project_category_filter
                                                             : "any");
    append_filter_button(out, out_size, &cursor, starts, ends, 2, label);
    (void)snprintf(label, sizeof(label), "Archived:%s",
                   state->include_archived_projects ? "yes" : "no");
    append_filter_button(out, out_size, &cursor, starts, ends, 3, label);
    append_filter_button(out, out_size, &cursor, starts, ends, 4, "Clear");
}

void draw_project_list(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    const unsigned rows = apl_tui_frame_layout(&state->frame)->body.rows;
    if (active_team_id(state) == 0) {
        apl_tui_frame_draw_line(body, 0, "No active team. Select a team before browsing projects.");
        return;
    }
    char header[128];
    (void)snprintf(header, sizeof(header), "Development: %zu project%s", state->project_count,
                   state->project_count == 1 ? "" : "s");
    apl_tui_frame_draw_line(body, 0, header);

    unsigned starts[5] = {};
    unsigned ends[5] = {};
    char filters[512];
    build_project_filter_bar(state, filters, sizeof(filters), starts, ends);
    apl_tui_frame_draw_line(body, 1, filters);

    if (state->project_count == 0) {
        apl_tui_frame_draw_line(
            body, 3,
            "No projects match. Press n to create one, f to change filters, or a for archived.");
        return;
    }
    const size_t visible = apl_tui_list_visible_count(&state->list);
    for (size_t offset = 0; offset < visible && offset + 3 < rows - 1; ++offset) {
        const size_t index = state->list.offset + offset;
        const apl_project *const project = &state->projects[index];
        const bool focused = index == state->list.selected;
        char season[APL_REFERENCE_LABEL_MAX];
        season_label(state, project->season_id, season, sizeof(season));
        char line[512];
        (void)snprintf(line, sizeof(line), "%c %.64s%s  [%s]  %s%s%s%s%s", focused ? '>' : ' ',
                       project->title, project->meta.archived ? " [archived]" : "",
                       project->status[0] != '\0' ? project->status : "status not recorded",
                       project->category[0] != '\0' ? project->category : "category not recorded",
                       season[0] != '\0' ? "  season: " : "", season,
                       project->target_date[0] != '\0' ? "  target: " : "", project->target_date);
        apl_tui_theme_apply(&state->theme, body,
                            focused ? APL_TUI_ROLE_SELECTION : APL_TUI_ROLE_BODY);
        apl_tui_frame_draw_line(body, (int)(offset + 3), line);
        apl_tui_theme_reset(body);
    }
}

void draw_project_detail(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    const apl_tui_layout *const layout = apl_tui_frame_layout(&state->frame);
    const unsigned cols = layout->body.cols;
    const unsigned rows = layout->body.rows;
    const apl_project *const project = &state->project_detail;
    int row = 0;
    char line[512];
    (void)snprintf(line, sizeof(line), "Project: %s%s", project->title,
                   project->meta.archived ? " [archived]" : "");
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Status: %s",
                   project->status[0] != '\0' ? project->status : "not recorded");
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Category: %s",
                   project->category[0] != '\0' ? project->category : "not recorded");
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Target date: %s",
                   project->target_date[0] != '\0' ? project->target_date : "not recorded");
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Actual date: %s",
                   project->actual_date[0] != '\0' ? project->actual_date : "not recorded");
    apl_tui_frame_draw_line(body, row++, line);
    char season[APL_REFERENCE_LABEL_MAX];
    char car[APL_REFERENCE_LABEL_MAX];
    season_label(state, project->season_id, season, sizeof(season));
    car_label(state, project->car_id, car, sizeof(car));
    (void)snprintf(line, sizeof(line), "Season: %s", season[0] != '\0' ? season : "not linked");
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Car: %s", car[0] != '\0' ? car : "not linked");
    apl_tui_frame_draw_line(body, row++, line);
    apl_tui_frame_draw_line(body, row++, "Description:");
    row += (int)apl_tui_frame_draw_wrapped(body, row, 2, cols > 2 ? cols - 2 : cols,
                                           project->description[0] != '\0' ? project->description
                                                                           : "not recorded");
    apl_tui_frame_draw_line(body, row++, "Notes:");
    row += (int)apl_tui_frame_draw_wrapped(body, row, 2, cols > 2 ? cols - 2 : cols,
                                           project->notes[0] != '\0' ? project->notes
                                                                     : "not recorded");
    ++row;

    apl_tui_frame_draw_line(body, row++, "Timeline (entered-date order):");
    if (state->project_update_count == 0) {
        apl_tui_frame_draw_line(body, row++, "  No updates recorded.");
    } else {
        for (size_t index = 0; index < state->project_update_count && row < (int)rows - 1;
             ++index) {
            const apl_project_update *const update = &state->project_updates[index];
            (void)snprintf(line, sizeof(line), "  %s  %.80s%s%s", update->update_date,
                           update->summary, update->status_change[0] != '\0' ? "  -> " : "",
                           update->status_change);
            apl_tui_frame_draw_line(body, row++, line);
            if (update->notes[0] != '\0' && row < (int)rows - 1) {
                (void)snprintf(line, sizeof(line), "      notes: %.100s", update->notes);
                apl_tui_frame_draw_line(body, row++, line);
            }
        }
    }
    ++row;

    apl_tui_frame_draw_line(body, row++, "Linked ledger entries (ledger-derived actuals):");
    if (state->project_entry_count == 0) {
        apl_tui_frame_draw_line(body, row++, "  No linked ledger entries. Cost is not recorded.");
    } else {
        for (size_t index = 0; index < state->project_entry_count && row < (int)rows - 1; ++index) {
            const apl_ledger_entry *const entry = &state->project_entries[index];
            const bool focused = index == state->list.selected;
            char amount[64];
            (void)apl_money_format(&entry->amount, amount, sizeof(amount));
            (void)snprintf(line, sizeof(line), "%c %.10s  %-16.16s  %-24.24s %.120s  %s",
                           focused ? '>' : ' ', entry->entry_date,
                           apl_ledger_kind_label(entry->kind), entry->category, entry->description,
                           amount);
            apl_tui_theme_apply(&state->theme, body,
                                focused ? APL_TUI_ROLE_SELECTION : APL_TUI_ROLE_BODY);
            apl_tui_frame_draw_line(body, row++, line);
            apl_tui_theme_reset(body);
        }
        char amount[64];
        (void)apl_money_format(&state->project_cost.actual_cost, amount, sizeof(amount));
        (void)snprintf(line, sizeof(line), "Actual cost (ledger-derived): %s  (%zu linked row%s)",
                       amount, state->project_cost.entry_count,
                       state->project_cost.entry_count == 1 ? "" : "s");
        apl_tui_frame_draw_line(body, row++, line);
    }
    if (row < (int)rows - 1) {
        (void)snprintf(line, sizeof(line), "Created: %s   Updated: %s", project->meta.created_at,
                       project->meta.updated_at);
        apl_tui_frame_draw_line(body, row++, line);
    }
}

void handle_project_list_input(shell_state *const state, const uint32_t id)
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
        if (active_team_id(state) == 0) {
            set_status_warning(state, "Select an active team before creating a project.");
        } else {
            open_project_dialog(state, false);
        }
        return;
    case 'f':
    case 'F':
        open_project_filter_dialog(state);
        return;
    case 'a':
    case 'A':
        state->include_archived_projects = !state->include_archived_projects;
        if (load_projects(state, 0, &error) != APL_OK) {
            show_error_dialog(state, &error);
        } else {
            set_status(state, state->include_archived_projects ? "Including archived projects."
                                                               : "Hiding archived projects.");
        }
        return;
    case 'c':
    case 'C':
        state->project_status_filter[0] = '\0';
        state->project_category_filter[0] = '\0';
        state->project_season_filter = 0;
        if (load_projects(state, 0, &error) != APL_OK) {
            show_error_dialog(state, &error);
        } else {
            set_status(state, "Project filters cleared.");
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
        open_selected_project(state);
    }
}

void handle_project_detail_input(shell_state *const state, const uint32_t id)
{
    apl_error error;
    apl_error_init(&error);
    if (handle_body_list_navigation(state, id)) {
        return;
    }
    switch (id) {
    case NCKEY_ESC:
        if (load_projects(state, state->project_detail.id, &error) != APL_OK) {
            show_error_dialog(state, &error);
            return;
        }
        state->screen = SHELL_SCREEN_PROJECT_LIST;
        return;
    case 'e':
    case 'E':
        if (state->project_detail.meta.archived) {
            set_status_warning(state, "Archived projects are read-only in this view.");
        } else {
            open_project_dialog(state, true);
        }
        return;
    case 'u':
    case 'U':
        if (state->project_detail.meta.archived) {
            set_status_warning(state, "Archived projects cannot receive new updates.");
        } else {
            open_project_update_dialog(state);
        }
        return;
    case 'x':
    case 'X':
        if (state->project_detail.meta.archived) {
            set_status_warning(state, "This project is already archived.");
        } else {
            state->pending = SHELL_PENDING_ARCHIVE_PROJECT;
            state->pending_record_id = state->project_detail.id;
            apl_tui_dialog_open_confirm(&state->dialog, "Archive project?",
                                        "The project and its timeline remain readable. No records "
                                        "are deleted and no completion date is invented.");
            apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
        }
        return;
    case 'c':
    case 'C':
        open_money_entry_dialog_for_project(state, APL_LEDGER_KIND_EXPENSE,
                                            state->project_detail.title);
        return;
    case 'b':
    case 'B':
        state->project_return_pending = false;
        state->budget_project_filter = state->project_detail.id;
        state->include_archived_budgets = false;
        state->section = APL_TUI_SECTION_MONEY;
        state->screen = SHELL_SCREEN_BUDGET_LIST;
        if (load_budget_report(state, &error) != APL_OK) {
            show_error_dialog(state, &error);
            return;
        }
        select_first_budget_plan(state);
        sync_status_context(state);
        set_status(state,
                   "Budget plans for project '%s'. n creates a linked plan; Enter edits a "
                   "plan row.",
                   state->project_detail.title);
        return;
    case 'q':
    case 'Q':
        state->quit = true;
        return;
    default:
        break;
    }
    if (is_enter_key(id) && state->project_entry_count > 0 &&
        state->list.selected < state->project_entry_count) {
        const apl_ledger_entry selected = state->project_entries[state->list.selected];
        state->entry_detail = selected;
        if (load_entry_activity(state, selected.id, &error) != APL_OK) {
            show_error_dialog(state, &error);
            return;
        }
        state->project_return_pending = true;
        state->project_return_section = APL_TUI_SECTION_DEVELOPMENT;
        state->project_return_screen = SHELL_SCREEN_PROJECT_DETAIL;
        state->project_return_id = state->project_detail.id;
        state->section = APL_TUI_SECTION_MONEY;
        state->screen = SHELL_SCREEN_MONEY_DETAIL;
        sync_status_context(state);
        set_status(state, "Linked ledger entry. Esc returns to the project.");
    }
}

apl_status project_resolve_season(shell_state *const state, const char *const text,
                                  int64_t *const out, apl_error *const error)
{
    *out = 0;
    char normalized[APL_REFERENCE_LABEL_MAX];
    apl_status result =
        apl_reference_optional_normalize(text, "season", normalized, sizeof(normalized), error);
    if (result != APL_OK) {
        apl_error_set_field(error, APL_FIELD_SEASON);
        return result;
    }
    if (normalized[0] == '\0') {
        return APL_OK;
    }
    const int64_t team_id = active_team_id(state);
    if (team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "select an active team before linking a season");
        apl_error_set_field(error, APL_FIELD_SEASON);
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_season *seasons = nullptr;
    size_t count = 0;
    result = apl_season_service_list(state->app, team_id, true, &seasons, &count, error);
    if (result != APL_OK) {
        apl_error_set_field(error, APL_FIELD_SEASON);
        return result;
    }
    for (size_t index = 0; index < count; ++index) {
        if (apl_tui_text_equal_ci(normalized, seasons[index].label)) {
            *out = seasons[index].id;
            break;
        }
    }
    apl_season_service_free_list(seasons);
    if (*out == 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "season '%s' was not found for this team; leave the field blank or use an "
                      "existing season",
                      normalized);
        apl_error_set_field(error, APL_FIELD_SEASON);
        return APL_ERR_INVALID_ARGUMENT;
    }
    return APL_OK;
}

apl_status project_resolve_car(shell_state *const state, const char *const text, int64_t *const out,
                               apl_error *const error)
{
    *out = 0;
    char normalized[APL_REFERENCE_LABEL_MAX];
    apl_status result =
        apl_reference_optional_normalize(text, "car", normalized, sizeof(normalized), error);
    if (result != APL_OK) {
        apl_error_set_field(error, APL_FIELD_CAR);
        return result;
    }
    if (normalized[0] == '\0') {
        return APL_OK;
    }
    const int64_t team_id = active_team_id(state);
    if (team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "select an active team before linking a car");
        apl_error_set_field(error, APL_FIELD_CAR);
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_car *cars = nullptr;
    size_t count = 0;
    result = apl_car_service_list(state->app, team_id, true, &cars, &count, error);
    if (result != APL_OK) {
        apl_error_set_field(error, APL_FIELD_CAR);
        return result;
    }
    for (size_t index = 0; index < count; ++index) {
        if (apl_tui_text_equal_ci(normalized, cars[index].label)) {
            *out = cars[index].id;
            break;
        }
    }
    apl_car_service_free_list(cars);
    if (*out == 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "car '%s' was not found for this team; leave the field blank or use an "
                      "existing car",
                      normalized);
        apl_error_set_field(error, APL_FIELD_CAR);
        return APL_ERR_INVALID_ARGUMENT;
    }
    return APL_OK;
}

apl_status project_resolve_title(shell_state *const state, const char *const text,
                                 int64_t *const out, apl_error *const error)
{
    *out = 0;
    char normalized[APL_REFERENCE_LABEL_MAX];
    apl_status result =
        apl_reference_optional_normalize(text, "project", normalized, sizeof(normalized), error);
    if (result != APL_OK) {
        apl_error_set_field(error, APL_FIELD_PROJECT);
        return result;
    }
    if (normalized[0] == '\0') {
        return APL_OK;
    }
    const int64_t team_id = active_team_id(state);
    if (team_id <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "select an active team before linking a project");
        apl_error_set_field(error, APL_FIELD_PROJECT);
        return APL_ERR_INVALID_ARGUMENT;
    }
    apl_project_filter filter = {.team_id = team_id, .include_archived = true};
    apl_project *projects = nullptr;
    size_t count = 0;
    result = apl_project_service_list(state->app, &filter, &projects, &count, error);
    if (result != APL_OK) {
        apl_error_set_field(error, APL_FIELD_PROJECT);
        return result;
    }
    for (size_t index = 0; index < count; ++index) {
        if (apl_tui_text_equal_ci(normalized, projects[index].title)) {
            *out = projects[index].id;
            break;
        }
    }
    apl_project_service_free_list(projects);
    if (*out == 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                      "project '%s' was not found for this team; leave the field blank or use an "
                      "existing project",
                      normalized);
        apl_error_set_field(error, APL_FIELD_PROJECT);
        return APL_ERR_INVALID_ARGUMENT;
    }
    return APL_OK;
}

static void open_project_form(shell_state *const state, const bool edit, const char *const title,
                              const char *const category, const char *const description,
                              const char *const status, const char *const target_date,
                              const char *const actual_date, const char *const season_name,
                              const char *const car_name, const char *const notes)
{
    apl_tui_dialog_open_form(
        &state->dialog, edit ? "Edit project" : "Create project",
        "Title is required. Category/status are free text. Season/car links use same-team names.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Title", title, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Category (optional)", category, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Description (optional)", description, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Status (optional)", status, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Target date (YYYY-MM-DD)", target_date, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Actual date (YYYY-MM-DD)", actual_date, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Season (optional)", season_name, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Car (optional)", car_name, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes (optional)", notes, false);
    state->dialog_context =
        edit ? SHELL_DIALOG_CONTEXT_PROJECT_EDIT : SHELL_DIALOG_CONTEXT_PROJECT_CREATE;
    state->dialog_project_id = edit ? state->project_detail.id : 0;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter saves; Esc cancels without writing.");
}

void open_project_dialog(shell_state *const state, const bool edit)
{
    const apl_project *const project = edit ? &state->project_detail : nullptr;
    char season[APL_REFERENCE_LABEL_MAX] = "";
    char car[APL_REFERENCE_LABEL_MAX] = "";
    if (project != nullptr) {
        season_label(state, project->season_id, season, sizeof(season));
        car_label(state, project->car_id, car, sizeof(car));
    }
    open_project_form(
        state, edit, project != nullptr ? project->title : "",
        project != nullptr ? project->category : "", project != nullptr ? project->description : "",
        project != nullptr ? project->status : "", project != nullptr ? project->target_date : "",
        project != nullptr ? project->actual_date : "", season, car,
        project != nullptr ? project->notes : "");
}

void open_project_create_form_prefilled(shell_state *const state, const char *const title,
                                        const char *const category, const char *const description,
                                        const char *const status, const char *const target_date,
                                        const char *const actual_date,
                                        const char *const season_name, const char *const car_name,
                                        const char *const notes)
{
    open_project_form(state, false, title, category, description, status, target_date, actual_date,
                      season_name, car_name, notes);
}

void open_project_update_dialog(shell_state *const state)
{
    apl_tui_dialog_open_form(&state->dialog, "Log project update",
                             "Update date and summary are required. Status is optional free text.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Update date (YYYY-MM-DD)", "", true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Summary", "", true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Status change (optional)", "", false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes (optional)", "", false);
    state->dialog_context = SHELL_DIALOG_CONTEXT_PROJECT_UPDATE;
    state->dialog_project_id = state->project_detail.id;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter logs the update; Esc writes nothing.");
}

void open_project_filter_dialog(shell_state *const state)
{
    apl_tui_dialog_open_form(
        &state->dialog, "Filter projects",
        "Status, season, and category are matched case-insensitively by the names shown in the "
        "project list.");
    char season[APL_REFERENCE_LABEL_MAX];
    season_label(state, state->project_season_filter, season, sizeof(season));
    (void)apl_tui_dialog_add_field(&state->dialog, "Status (optional)",
                                   state->project_status_filter, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Season (optional)", season, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Category (optional)",
                                   state->project_category_filter, false);
    state->dialog_context = SHELL_DIALOG_CONTEXT_PROJECT_FILTER;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Enter applies the filters; Esc leaves them unchanged.");
}

unsigned project_form_error_field(const apl_error *const error)
{
    switch (apl_error_field_id(error)) {
    case APL_FIELD_TITLE:
        return 0;
    case APL_FIELD_CATEGORY:
        return 1;
    case APL_FIELD_DESCRIPTION:
        return 2;
    case APL_FIELD_PROJECT_STATUS:
        return 3;
    case APL_FIELD_TARGET_DATE:
        return 4;
    case APL_FIELD_ACTUAL_DATE:
        return 5;
    case APL_FIELD_SEASON:
        return 6;
    case APL_FIELD_CAR:
        return 7;
    case APL_FIELD_NOTES:
        return 8;
    default:
        return 0;
    }
}

unsigned project_update_form_error_field(const apl_error *const error)
{
    switch (apl_error_field_id(error)) {
    case APL_FIELD_UPDATE_DATE:
        return 0;
    case APL_FIELD_SUMMARY:
        return 1;
    case APL_FIELD_PROJECT_STATUS:
        return 2;
    case APL_FIELD_NOTES:
        return 3;
    default:
        return 0;
    }
}

bool perform_project_create(shell_state *const state, const char *const title,
                            const char *const category, const char *const description,
                            const char *const status, const char *const target_date,
                            const char *const actual_date, const char *const season_name,
                            const char *const car_name, const char *const notes,
                            apl_error *const error)
{
    int64_t season = 0;
    int64_t car = 0;
    apl_status result = project_resolve_season(state, season_name, &season, error);
    if (result == APL_OK) {
        result = project_resolve_car(state, car_name, &car, error);
    }
    if (result != APL_OK) {
        set_status_from_error(state, "Cannot create project", error);
        return false;
    }
    apl_project created = {};
    result = apl_project_service_create(state->app, active_team_id(state), title, category,
                                        description, status, target_date, actual_date, season, car,
                                        notes, &created, error);
    if (result != APL_OK) {
        set_status_from_error(state, "Cannot create project", error);
        return false;
    }
    state->project_detail = created;
    if (load_projects(state, created.id, error) != APL_OK ||
        load_project_detail(state, created.id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    state->section = APL_TUI_SECTION_DEVELOPMENT;
    state->screen = SHELL_SCREEN_PROJECT_DETAIL;
    sync_status_context(state);
    set_status_success(state, "Project '%s' created.", created.title);
    return true;
}

bool perform_project_update(shell_state *const state, const char *const title,
                            const char *const category, const char *const description,
                            const char *const status, const char *const target_date,
                            const char *const actual_date, const char *const season_name,
                            const char *const car_name, const char *const notes,
                            apl_error *const error)
{
    int64_t season = 0;
    int64_t car = 0;
    apl_status result = project_resolve_season(state, season_name, &season, error);
    if (result == APL_OK) {
        result = project_resolve_car(state, car_name, &car, error);
    }
    if (result != APL_OK) {
        set_status_from_error(state, "Cannot save project", error);
        return false;
    }
    apl_project updated = {};
    result = apl_project_service_update(state->app, active_team_id(state), state->dialog_project_id,
                                        title, category, description, status, target_date,
                                        actual_date, season, car, notes, &updated, error);
    if (result != APL_OK) {
        set_status_from_error(state, "Cannot save project", error);
        return false;
    }
    state->project_detail = updated;
    if (load_projects(state, updated.id, error) != APL_OK ||
        load_project_detail(state, updated.id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    state->screen = SHELL_SCREEN_PROJECT_DETAIL;
    set_status_success(state, "Project changes saved.");
    return true;
}

bool perform_project_log_update(shell_state *const state, const char *const update_date,
                                const char *const summary, const char *const status_change,
                                const char *const notes, apl_error *const error)
{
    apl_project_update stored = {};
    apl_project updated = {};
    const apl_status result = apl_project_service_log_update(
        state->app, active_team_id(state), state->dialog_project_id, update_date, summary,
        status_change, notes, &stored, &updated, error);
    if (result != APL_OK) {
        set_status_from_error(state, "Cannot log project update", error);
        return false;
    }
    state->project_detail = updated;
    if (load_projects(state, updated.id, error) != APL_OK ||
        load_project_detail(state, updated.id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    state->screen = SHELL_SCREEN_PROJECT_DETAIL;
    set_status_success(state, "Update logged for %s.", stored.update_date);
    return true;
}

void perform_project_archive(shell_state *const state, const int64_t id, apl_error *const error)
{
    apl_project archived = {};
    if (apl_project_service_archive(state->app, active_team_id(state), id, &archived, error) !=
        APL_OK) {
        show_error_dialog(state, error);
        return;
    }
    if (load_projects(state, 0, error) != APL_OK) {
        show_error_dialog(state, error);
        return;
    }
    state->screen = SHELL_SCREEN_PROJECT_LIST;
    set_status_success(state, "Project archived. It remains readable with its timeline.");
}
