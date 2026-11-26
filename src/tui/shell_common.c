#include "tui/shell_internal.h"

void set_status_level(shell_state *const state, const apl_tui_status_level level,
                      const char *const format, va_list args)
{
    char message[APL_TUI_STATUS_MESSAGE_MAX];
    (void)vsnprintf(message, sizeof(message), format, args);
    (void)apl_tui_status_set_message(&state->status, level, "%s", message);
}

[[gnu::format(printf, 2, 3)]]
void set_status(shell_state *const state, const char *const format, ...)
{
    va_list args;
    va_start(args);
    set_status_level(state, APL_TUI_STATUS_INFO, format, args);
    va_end(args);
}

[[gnu::format(printf, 2, 3)]]
void set_status_success(shell_state *const state, const char *const format, ...)
{
    va_list args;
    va_start(args);
    set_status_level(state, APL_TUI_STATUS_SUCCESS, format, args);
    va_end(args);
}

[[gnu::format(printf, 2, 3)]]
void set_status_warning(shell_state *const state, const char *const format, ...)
{
    va_list args;
    va_start(args);
    set_status_level(state, APL_TUI_STATUS_WARNING, format, args);
    va_end(args);
}

[[gnu::format(printf, 2, 3)]]
void set_status_error(shell_state *const state, const char *const format, ...)
{
    va_list args;
    va_start(args);
    set_status_level(state, APL_TUI_STATUS_ERROR, format, args);
    va_end(args);
}

void set_status_from_error(shell_state *const state, const char *const context,
                           const apl_error *const error)
{
    const char *const message = apl_error_message(error);
    if (message != nullptr && message[0] != '\0') {
        (void)apl_tui_status_set_message(&state->status, APL_TUI_STATUS_ERROR, "%s: %s", context,
                                         message);
    } else {
        (void)apl_tui_status_set_message(&state->status, APL_TUI_STATUS_ERROR, "%s", context);
    }
}

size_t find_team_index(const shell_state *const state, const int64_t id)
{
    for (size_t index = 0; index < state->team_count; ++index) {
        if (state->teams[index].id == id) {
            return index;
        }
    }
    return SIZE_MAX;
}

const apl_team *selected_team(const shell_state *const state)
{
    if (state->teams == nullptr || state->team_count == 0 ||
        state->list.selected >= state->team_count) {
        return nullptr;
    }
    return &state->teams[state->list.selected];
}

bool active_status_has_team(const apl_active_team_status status)
{
    return status == APL_ACTIVE_TEAM_ACTIVE || status == APL_ACTIVE_TEAM_FALLBACK;
}

size_t non_archived_count(const shell_state *const state)
{
    size_t count = 0;
    for (size_t index = 0; index < state->team_count; ++index) {
        if (!state->teams[index].meta.archived) {
            ++count;
        }
    }
    return count;
}

bool can_archive_team(const shell_state *const state, const apl_team *const team)
{
    return team != nullptr && !team->meta.archived && non_archived_count(state) > 1;
}

unsigned form_error_field(const apl_error *const error)
{
    return (apl_error_field_id(error) == APL_FIELD_NOTES) ? 1 : 0;
}

unsigned reference_form_error_field(const shell_dialog_context context,
                                    const apl_error *const error)
{
    const apl_field field = apl_error_field_id(error);
    switch (context) {
    case SHELL_DIALOG_CONTEXT_SEASON_CREATE:
    case SHELL_DIALOG_CONTEXT_SEASON_EDIT:
        switch (field) {
        case APL_FIELD_LABEL:
            return 0;
        case APL_FIELD_START_DATE:
            return 1;
        case APL_FIELD_END_DATE:
            return 2;
        case APL_FIELD_NOTES:
            return 3;
        default:
            return 0;
        }
    case SHELL_DIALOG_CONTEXT_CAR_CREATE:
    case SHELL_DIALOG_CONTEXT_CAR_EDIT:
        switch (field) {
        case APL_FIELD_LABEL:
            return 0;
        case APL_FIELD_CHASSIS:
            return 1;
        case APL_FIELD_NOTES:
            return 2;
        default:
            return 0;
        }
    case SHELL_DIALOG_CONTEXT_DRIVER_CREATE:
    case SHELL_DIALOG_CONTEXT_DRIVER_EDIT:
        switch (field) {
        case APL_FIELD_NAME:
            return 0;
        case APL_FIELD_SHORT_NAME:
            return 1;
        case APL_FIELD_NOTES:
            return 2;
        default:
            return 0;
        }
    case SHELL_DIALOG_CONTEXT_MEMBERSHIP_ADD:
        switch (field) {
        case APL_FIELD_START_DATE:
            return 0;
        case APL_FIELD_END_DATE:
            return 1;
        case APL_FIELD_ROLE:
            return 2;
        case APL_FIELD_NOTES:
            return 3;
        default:
            return 0;
        }
    case SHELL_DIALOG_CONTEXT_MEMBERSHIP_END:
        return 1;
    default:
        return 0;
    }
}

unsigned money_form_error_field(const apl_error *const error)
{
    switch (apl_error_field_id(error)) {
    case APL_FIELD_DATE:
        return 1;
    case APL_FIELD_CATEGORY:
        return 2;
    case APL_FIELD_DESCRIPTION:
        return 3;
    case APL_FIELD_PROJECT:
        return 4;
    case APL_FIELD_AMOUNT:
    case APL_FIELD_CURRENCY:
    default:
        return 0;
    }
}

unsigned budget_form_error_field(const apl_error *const error)
{
    switch (apl_error_field_id(error)) {
    case APL_FIELD_START_DATE:
    case APL_FIELD_PERIOD:
        return 0;
    case APL_FIELD_END_DATE:
        return 1;
    case APL_FIELD_CATEGORY:
        return 2;
    case APL_FIELD_AMOUNT:
        return 3;
    case APL_FIELD_PROJECT:
        return 4;
    case APL_FIELD_NOTES:
        return 5;
    default:
        return 0;
    }
}

apl_status refresh_active(shell_state *const state, apl_error *const error)
{
    const apl_status status =
        apl_team_service_active(state->app, &state->active_team, &state->active_status, error);
    if (status == APL_OK) {
        if (active_status_has_team(state->active_status)) {
            apl_tui_status_set_team(&state->status, state->active_team.name);
        } else {
            apl_tui_status_clear_team(&state->status);
        }
    }
    return status;
}

void sync_status_context(shell_state *const state)
{
    const char *const season = selected_season_label(state);
    if (season != nullptr && season[0] != '\0') {
        apl_tui_status_set_season(&state->status, season);
    } else {
        apl_tui_status_clear_season(&state->status);
    }
    apl_tui_status_set_context(&state->status, apl_tui_section_label(state->section));
}

apl_status detect_any_teams(shell_state *const state, apl_error *const error)
{
    apl_team *teams = nullptr;
    size_t count = 0;
    const apl_status status = apl_team_service_list(state->app, true, &teams, &count, error);
    if (status != APL_OK) {
        return status;
    }
    state->any_teams = count > 0;
    apl_team_service_free_list(teams);
    return APL_OK;
}

int64_t active_team_id(const shell_state *const state)
{
    return active_status_has_team(state->active_status) ? state->active_team.id : 0;
}

void free_reference_lists(shell_state *const state)
{
    apl_season_service_free_list(state->seasons);
    state->seasons = nullptr;
    state->season_count = 0;
    apl_car_service_free_list(state->cars);
    state->cars = nullptr;
    state->car_count = 0;
    apl_driver_service_free_list(state->drivers);
    state->drivers = nullptr;
    state->driver_count = 0;
    apl_membership_service_free_list(state->memberships);
    state->memberships = nullptr;
    state->membership_count = 0;
    apl_activity_service_free_events(state->events);
    state->events = nullptr;
    state->event_count = 0;
    apl_ledger_service_free_list(state->entries);
    state->entries = nullptr;
    state->entry_count = 0;
    apl_budget_service_free_report(state->budget_rows);
    state->budget_rows = nullptr;
    state->budget_row_count = 0;
    apl_activity_service_free_events(state->entry_events);
    state->entry_events = nullptr;
    state->entry_event_count = 0;
    apl_project_service_free_list(state->projects);
    state->projects = nullptr;
    state->project_count = 0;
    apl_project_service_free_updates(state->project_updates);
    state->project_updates = nullptr;
    state->project_update_count = 0;
    apl_ledger_service_free_list(state->project_entries);
    state->project_entries = nullptr;
    state->project_entry_count = 0;
    apl_part_type_service_free_list(state->part_types);
    state->part_types = nullptr;
    state->part_type_count = 0;
    apl_part_unit_service_free_list(state->part_units);
    state->part_units = nullptr;
    state->part_unit_count = 0;
    apl_lifecycle_service_free_fitment(state->fitment_rows);
    state->fitment_rows = nullptr;
    state->fitment_count = 0;
    apl_lifecycle_service_free_history(state->unit_events);
    state->unit_events = nullptr;
    state->unit_event_count = 0;
}

apl_status refresh_teams(shell_state *const state, const int64_t preferred_id,
                         apl_error *const error)
{
    int64_t keep_id = preferred_id;
    const apl_team *current = selected_team(state);
    if (keep_id == 0 && current != nullptr) {
        keep_id = current->id;
    }

    apl_team *teams = nullptr;
    size_t count = 0;
    const apl_status status =
        apl_team_service_list(state->app, state->include_archived, &teams, &count, error);
    if (status != APL_OK) {
        return status;
    }

    apl_team_service_free_list(state->teams);
    state->teams = teams;
    state->team_count = count;
    apl_tui_list_set_count(&state->list, count);

    if (keep_id != 0) {
        const size_t index = find_team_index(state, keep_id);
        if (index != SIZE_MAX) {
            apl_tui_list_select(&state->list, index);
        }
    }
    return APL_OK;
}

void show_error_dialog(shell_state *const state, const apl_error *const error)
{
    set_status_from_error(state, "Operation failed", error);
    apl_tui_dialog_open_error(&state->dialog, "Operation failed", apl_error_message(error));
    state->dialog_context = SHELL_DIALOG_CONTEXT_NONE;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    state->pending = SHELL_PENDING_NONE;
}

void show_info_dialog(shell_state *const state, const char *const title, const char *const message)
{
    apl_tui_dialog_open_error(&state->dialog, title, message);
    state->dialog_context = SHELL_DIALOG_CONTEXT_NONE;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    state->pending = SHELL_PENDING_NONE;
}

void close_shell_dialog(shell_state *const state)
{
    apl_tui_dialog_init(&state->dialog);
    state->dialog_context = SHELL_DIALOG_CONTEXT_NONE;
    free_picker_teams(state);
    apl_tui_focus_pop(&state->focus);
}

void append_filter_button(char *const out, const size_t out_size, unsigned *const cursor,
                          unsigned starts[4], unsigned ends[4], const unsigned index,
                          const char *const label)
{
    starts[index] = *cursor;
    (void)apl_tui_text_append(out, out_size - *cursor, label);
    *cursor = (unsigned)strlen(out);
    ends[index] = *cursor;
    (void)apl_tui_text_append(out, out_size - *cursor, "  ");
    *cursor = (unsigned)strlen(out);
}

bool is_enter_key(const uint32_t id)
{
    return id == NCKEY_ENTER || id == '\n' || id == '\r';
}

bool text_is_numeric(const char *const text)
{
    if (text == nullptr || text[0] == '\0') {
        return false;
    }
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        if (*cursor < '0' || *cursor > '9') {
            return false;
        }
    }
    return true;
}

int64_t now_ms(void)
{
    struct timespec now = {};
    (void)timespec_get(&now, TIME_UTC);
    return (int64_t)now.tv_sec * 1000 + (int64_t)(now.tv_nsec / 1000000);
}

bool point_in_rect(const apl_tui_rect rect, const int y, const int x)
{
    return y >= (int)rect.y && y < (int)(rect.y + rect.rows) && x >= (int)rect.x &&
           x < (int)(rect.x + rect.cols);
}

bool handle_body_list_navigation(shell_state *const state, const uint32_t id)
{
    switch (id) {
    case NCKEY_UP:
        apl_tui_list_move(&state->list, -1);
        return true;
    case NCKEY_DOWN:
        apl_tui_list_move(&state->list, 1);
        return true;
    case NCKEY_PGUP:
        apl_tui_list_page(&state->list, -1);
        return true;
    case NCKEY_PGDOWN:
        apl_tui_list_page(&state->list, 1);
        return true;
    case NCKEY_HOME:
        apl_tui_list_home(&state->list);
        return true;
    case NCKEY_END:
        apl_tui_list_end(&state->list);
        return true;
    default:
        return false;
    }
}

void shell_save_view_state(shell_state *const state, const apl_tui_section section)
{
    const size_t index = apl_tui_section_tab_index(section);
    if (index != SIZE_MAX) {
        state->section_lists[index] = state->list;
    }
}

void shell_restore_view_state(shell_state *const state, const apl_tui_section section)
{
    const size_t index = apl_tui_section_tab_index(section);
    if (index != SIZE_MAX) {
        state->list = state->section_lists[index];
    }
}
