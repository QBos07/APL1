#include "tui/shell.h"

#include <limits.h>
#include <notcurses/notcurses.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app/activity_service.h"
#include "app/car_service.h"
#include "app/driver_service.h"
#include "app/membership_service.h"
#include "app/season_service.h"
#include "app/team_service.h"
#include "app/version.h"
#include "tui/action.h"
#include "tui/command.h"
#include "tui/command_line.h"
#include "tui/dialog.h"
#include "tui/focus.h"
#include "tui/frame.h"
#include "tui/list.h"
#include "tui/menu.h"
#include "tui/navigation.h"
#include "tui/status.h"
#include "tui/text.h"
#include "tui/theme.h"

typedef enum shell_screen {
    SHELL_SCREEN_WELCOME = 0,
    SHELL_SCREEN_TEAM_LIST,
    SHELL_SCREEN_TEAM_DETAIL,
    SHELL_SCREEN_SEASON_LIST,
    SHELL_SCREEN_SEASON_DETAIL,
    SHELL_SCREEN_CAR_LIST,
    SHELL_SCREEN_CAR_DETAIL,
    SHELL_SCREEN_PEOPLE_LIST,
    SHELL_SCREEN_PERSON_DETAIL,
    SHELL_SCREEN_HISTORY_LIST,
    SHELL_SCREEN_PLACEHOLDER,
} shell_screen;

typedef enum shell_pending_action {
    SHELL_PENDING_NONE = 0,
    SHELL_PENDING_ARCHIVE_TEAM,
    SHELL_PENDING_ARCHIVE_SEASON,
    SHELL_PENDING_ARCHIVE_CAR,
    SHELL_PENDING_ARCHIVE_DRIVER,
} shell_pending_action;

typedef enum shell_dialog_context {
    SHELL_DIALOG_CONTEXT_NONE = 0,
    SHELL_DIALOG_CONTEXT_TEAM_PICKER,
    SHELL_DIALOG_CONTEXT_TEAM_CREATE,
    SHELL_DIALOG_CONTEXT_TEAM_EDIT,
    SHELL_DIALOG_CONTEXT_SEASON_CREATE,
    SHELL_DIALOG_CONTEXT_SEASON_EDIT,
    SHELL_DIALOG_CONTEXT_CAR_CREATE,
    SHELL_DIALOG_CONTEXT_CAR_EDIT,
    SHELL_DIALOG_CONTEXT_DRIVER_CREATE,
    SHELL_DIALOG_CONTEXT_DRIVER_EDIT,
    SHELL_DIALOG_CONTEXT_MEMBERSHIP_ADD,
    SHELL_DIALOG_CONTEXT_MEMBERSHIP_END,
    SHELL_DIALOG_CONTEXT_DRIVER_PICKER,
    SHELL_DIALOG_CONTEXT_ACTIVITY_DETAIL,
} shell_dialog_context;

constexpr int APL_TUI_SCROLL_STEP = 3;
constexpr int64_t APL_TUI_DOUBLE_CLICK_MS = 500;
constexpr size_t APL_TUI_HISTORY_FILTERS = 5;

typedef struct shell_state {
    apl_app *app;
    apl_team *teams;
    size_t team_count;
    apl_season *seasons;
    size_t season_count;
    int64_t selected_season_id;
    apl_car *cars;
    size_t car_count;
    apl_driver *drivers;
    size_t driver_count;
    apl_membership *memberships;
    size_t membership_count;
    apl_activity_event *events;
    size_t event_count;
    int64_t dialog_team_id;
    int64_t dialog_record_id;
    int64_t dialog_driver_id;
    apl_team *picker_teams;
    size_t picker_team_count;
    int64_t pending_team_id;
    int64_t pending_record_id;
    int64_t pending_driver_id;
    int64_t last_click_team_id;
    int64_t last_click_ms;
    int64_t last_mouse_ms;
    apl_tui_list list;
    apl_tui_list tabs;
    apl_activity_filter history_filter;
    apl_tui_frame frame;
    int64_t driver_choice_ids[APL_TUI_DIALOG_OPTIONS_MAX];
    apl_team active_team;
    apl_team detail;
    apl_season season_detail;
    apl_driver driver_detail;
    apl_car car_detail;
    apl_tui_section section;
    shell_screen screen;
    apl_active_team_status active_status;
    unsigned history_date_preset;
    unsigned history_action_filter;
    unsigned history_entity_filter;
    shell_dialog_context dialog_context;
    int driver_choice_mode;
    unsigned driver_choice_count;
    shell_pending_action pending;
    uint32_t last_mouse_id;
    int last_mouse_y;
    int last_mouse_x;
    apl_tui_focus focus;
    apl_tui_menu menu;
    apl_tui_status status;
    apl_tui_command_line command_line;
    apl_tui_dialog dialog;
    bool include_archived;
    bool any_teams;
    bool include_archived_seasons;
    bool include_archived_cars;
    bool include_archived_drivers;
    bool people_all_teams;
    bool history_all_teams;
    apl_tui_theme theme;
    bool picker_include_archived;
    bool team_focus;
    bool quit;
    char pending_start[APL_TUI_COMMAND_WORD_MAX];
    char pending_end[APL_TUI_COMMAND_WORD_MAX];
    char pending_role[APL_TUI_COMMAND_WORD_MAX];
    char pending_notes[APL_TUI_COMMAND_WORD_MAX];
} shell_state;

static void handle_dialog_input(shell_state *state, const ncinput *input);
static void open_selected_detail(shell_state *state);
static void open_selected_season(shell_state *state);
static void open_selected_car(shell_state *state);
static void open_selected_person(shell_state *state);
static void handle_people_list_input(shell_state *state, uint32_t id);
static void handle_person_detail_input(shell_state *state, uint32_t id);
static void handle_history_input(shell_state *state, uint32_t id);

static void set_status_level(shell_state *const state, const apl_tui_status_level level,
                             const char *const format, va_list args)
{
    char message[APL_TUI_STATUS_MESSAGE_MAX];
    (void)vsnprintf(message, sizeof(message), format, args);
    (void)apl_tui_status_set_message(&state->status, level, "%s", message);
}

static void set_status(shell_state *const state, const char *const format, ...)
{
    va_list args;
    va_start(args, format);
    set_status_level(state, APL_TUI_STATUS_INFO, format, args);
    va_end(args);
}

static void set_status_success(shell_state *const state, const char *const format, ...)
{
    va_list args;
    va_start(args, format);
    set_status_level(state, APL_TUI_STATUS_SUCCESS, format, args);
    va_end(args);
}

static void set_status_warning(shell_state *const state, const char *const format, ...)
{
    va_list args;
    va_start(args, format);
    set_status_level(state, APL_TUI_STATUS_WARNING, format, args);
    va_end(args);
}

static void set_status_error(shell_state *const state, const char *const format, ...)
{
    va_list args;
    va_start(args, format);
    set_status_level(state, APL_TUI_STATUS_ERROR, format, args);
    va_end(args);
}

static void set_status_from_error(shell_state *const state, const char *const context,
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

/** Find a team index by stable ID; SIZE_MAX means missing. */
static size_t find_team_index(const shell_state *const state, const int64_t id)
{
    for (size_t index = 0; index < state->team_count; ++index) {
        if (state->teams[index].id == id) {
            return index;
        }
    }
    return SIZE_MAX;
}

static const apl_team *selected_team(const shell_state *const state)
{
    if (state->teams == nullptr || state->team_count == 0 ||
        state->list.selected >= state->team_count) {
        return nullptr;
    }
    return &state->teams[state->list.selected];
}

static bool active_status_has_team(const apl_active_team_status status)
{
    return status == APL_ACTIVE_TEAM_ACTIVE || status == APL_ACTIVE_TEAM_FALLBACK;
}

/** Number of non-archived teams currently loaded. */
static size_t non_archived_count(const shell_state *const state)
{
    size_t count = 0;
    for (size_t index = 0; index < state->team_count; ++index) {
        if (!state->teams[index].archived) {
            ++count;
        }
    }
    return count;
}

static bool can_archive_team(const shell_state *const state, const apl_team *const team)
{
    return team != nullptr && !team->archived && non_archived_count(state) > 1;
}

/** Form field index for a business validation message. */
static unsigned form_error_field(const apl_error *const error)
{
    return (strstr(apl_error_message(error), "notes") != nullptr) ? 1 : 0;
}

static unsigned reference_form_error_field(const shell_dialog_context context,
                                           const apl_error *const error)
{
    const char *const message = apl_error_message(error);
    switch (context) {
    case SHELL_DIALOG_CONTEXT_SEASON_CREATE:
    case SHELL_DIALOG_CONTEXT_SEASON_EDIT:
        if (strstr(message, "label") != nullptr) {
            return 0;
        }
        if (strstr(message, "start") != nullptr) {
            return 1;
        }
        if (strstr(message, "end") != nullptr) {
            return 2;
        }
        return 3;
    case SHELL_DIALOG_CONTEXT_CAR_CREATE:
    case SHELL_DIALOG_CONTEXT_CAR_EDIT:
        if (strstr(message, "label") != nullptr) {
            return 0;
        }
        if (strstr(message, "chassis") != nullptr) {
            return 1;
        }
        return 2;
    case SHELL_DIALOG_CONTEXT_DRIVER_CREATE:
    case SHELL_DIALOG_CONTEXT_DRIVER_EDIT:
        if (strstr(message, "name") != nullptr) {
            return 0;
        }
        return 2;
    case SHELL_DIALOG_CONTEXT_MEMBERSHIP_ADD:
        if (strstr(message, "start") != nullptr) {
            return 0;
        }
        if (strstr(message, "end") != nullptr || strstr(message, "date") != nullptr) {
            return 1;
        }
        if (strstr(message, "role") != nullptr) {
            return 2;
        }
        return 3;
    case SHELL_DIALOG_CONTEXT_MEMBERSHIP_END:
        return 1;
    default:
        return 0;
    }
}

static apl_status refresh_active(shell_state *const state, apl_error *const error)
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

static const char *selected_season_label(const shell_state *const state)
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

static void sync_status_context(shell_state *const state)
{
    const char *const season = selected_season_label(state);
    if (season != nullptr && season[0] != '\0') {
        apl_tui_status_set_season(&state->status, season);
    } else {
        apl_tui_status_clear_season(&state->status);
    }
    apl_tui_status_set_context(&state->status, apl_tui_section_label(state->section));
}

static apl_status detect_any_teams(shell_state *const state, apl_error *const error)
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

/** Owning team for team-scoped views, or 0 when no active team exists. */
static int64_t active_team_id(const shell_state *const state)
{
    return active_status_has_team(state->active_status) ? state->active_team.id : 0;
}

static size_t find_season_index(const shell_state *const state, const int64_t id)
{
    for (size_t index = 0; index < state->season_count; ++index) {
        if (state->seasons[index].id == id) {
            return index;
        }
    }
    return SIZE_MAX;
}

static size_t find_car_index(const shell_state *const state, const int64_t id)
{
    for (size_t index = 0; index < state->car_count; ++index) {
        if (state->cars[index].id == id) {
            return index;
        }
    }
    return SIZE_MAX;
}

static size_t find_driver_index(const shell_state *const state, const int64_t id)
{
    for (size_t index = 0; index < state->driver_count; ++index) {
        if (state->drivers[index].id == id) {
            return index;
        }
    }
    return SIZE_MAX;
}

static void free_reference_lists(shell_state *const state)
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
}

static apl_status load_seasons(shell_state *const state, const int64_t preferred_id,
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

static apl_status load_cars(shell_state *const state, const int64_t preferred_id,
                            apl_error *const error)
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

static apl_status load_people(shell_state *const state, const int64_t preferred_id,
                              apl_error *const error)
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

static apl_status load_driver_memberships(shell_state *const state, const int64_t driver_id,
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

static const char *history_entity_filter_name(const unsigned index)
{
    static const char *const NAMES[] = {"", "team", "season", "driver", "membership", "car"};
    return (index < sizeof(NAMES) / sizeof(NAMES[0])) ? NAMES[index] : "";
}

static const char *history_action_filter_name(const unsigned index)
{
    static const char *const NAMES[] = {
        "",
        "team_created",
        "team_renamed",
        "team_archived",
        "team_selected",
        "season_created",
        "season_updated",
        "season_archived",
        "car_created",
        "car_updated",
        "car_archived",
        "driver_created",
        "driver_updated",
        "driver_archived",
        "membership_added",
        "membership_updated",
        "membership_ended",
    };
    return (index < sizeof(NAMES) / sizeof(NAMES[0])) ? NAMES[index] : "";
}

static unsigned history_action_filter_count(void)
{
    unsigned count = 1; /* "all" plus every explicit action */
    while (history_action_filter_name(count)[0] != '\0') {
        ++count;
    }
    return count;
}

static apl_status load_history(shell_state *const state, apl_error *const error)
{
    apl_activity_filter filter = {};
    if (!state->history_all_teams) {
        filter.team_id = active_team_id(state);
        filter.include_teamless = true;
    }
    (void)snprintf(filter.entity_type, sizeof(filter.entity_type), "%s",
                   history_entity_filter_name(state->history_entity_filter));
    (void)snprintf(filter.action, sizeof(filter.action), "%s",
                   history_action_filter_name(state->history_action_filter));
    if (state->history_date_preset > 0) {
        const int days =
            (state->history_date_preset == 1) ? 7 : ((state->history_date_preset == 2) ? 30 : 365);
        apl_reference_date_today_minus_days(days, filter.from_date, sizeof(filter.from_date));
    }
    state->history_filter = filter;

    apl_activity_event *events = nullptr;
    size_t count = 0;
    apl_status status = APL_OK;
    if (state->history_all_teams || filter.team_id > 0) {
        status = apl_activity_service_list(state->app, &filter, &events, &count, error);
    }
    if (status != APL_OK) {
        return status;
    }
    apl_activity_service_free_events(state->events);
    state->events = events;
    state->event_count = count;
    apl_tui_list_set_count(&state->list, count);
    return APL_OK;
}

/** Reload the visible team list, preserving/choosing a stable team ID. */
static apl_status refresh_teams(shell_state *const state, const int64_t preferred_id,
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

static void open_create_dialog(shell_state *const state, const char *const name,
                               const char *const notes)
{
    apl_tui_dialog_open_form(&state->dialog, "Create team",
                             "Name is required and must be unique. Notes are optional.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Name", name, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes", notes, false);
    state->dialog_context = SHELL_DIALOG_CONTEXT_TEAM_CREATE;
    state->dialog_team_id = 0;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter saves; Esc cancels without writing.");
}

static void open_create_form(shell_state *const state)
{
    open_create_dialog(state, "", "");
}

static void open_edit_form(shell_state *const state)
{
    apl_tui_dialog_open_form(&state->dialog, "Edit team",
                             "Editing team. Enter saves; Esc cancels.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Name", state->detail.name, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes", state->detail.notes, false);
    state->dialog_context = SHELL_DIALOG_CONTEXT_TEAM_EDIT;
    state->dialog_team_id = state->detail.id;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter saves; Esc cancels without writing.");
}

static void show_error_dialog(shell_state *const state, const apl_error *const error)
{
    set_status_from_error(state, "Operation failed", error);
    apl_tui_dialog_open_error(&state->dialog, "Operation failed", apl_error_message(error));
    state->dialog_context = SHELL_DIALOG_CONTEXT_NONE;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    state->pending = SHELL_PENDING_NONE;
}

static void show_info_dialog(shell_state *const state, const char *const title,
                             const char *const message)
{
    apl_tui_dialog_open_error(&state->dialog, title, message);
    state->dialog_context = SHELL_DIALOG_CONTEXT_NONE;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    state->pending = SHELL_PENDING_NONE;
}

static bool perform_create(shell_state *const state, const char *const name,
                           const char *const notes, apl_error *const error)
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

static bool perform_update(shell_state *const state, const int64_t team_id, const char *const name,
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

static void perform_archive(shell_state *const state, const int64_t team_id, apl_error *const error)
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

static void perform_set_active(shell_state *const state, const int64_t team_id,
                               apl_error *const error)
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
    sync_status_context(state);
    set_status_success(state, "Active team: %s", state->active_team.name);
}

static void toggle_archived_view(shell_state *const state, apl_error *const error)
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

static void open_season_dialog(shell_state *const state, const bool edit, const char *const label,
                               const char *const start_date, const char *const end_date,
                               const char *const notes)
{
    apl_tui_dialog_open_form(&state->dialog, edit ? "Edit season/period" : "Create season/period",
                             "Label is required. Dates use YYYY-MM-DD or stay blank.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Label", label, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Start date", start_date, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "End date", end_date, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes", notes, false);
    state->dialog_context =
        edit ? SHELL_DIALOG_CONTEXT_SEASON_EDIT : SHELL_DIALOG_CONTEXT_SEASON_CREATE;
    state->dialog_record_id = edit ? state->season_detail.id : 0;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter saves; Esc cancels without writing.");
}

static void open_car_dialog(shell_state *const state, const bool edit, const char *const label,
                            const char *const chassis, const char *const notes)
{
    apl_tui_dialog_open_form(&state->dialog, edit ? "Edit car/chassis" : "Create car/chassis",
                             "Identifier is required. Chassis and notes are optional.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Identifier", label, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Chassis", chassis, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes", notes, false);
    state->dialog_context = edit ? SHELL_DIALOG_CONTEXT_CAR_EDIT : SHELL_DIALOG_CONTEXT_CAR_CREATE;
    state->dialog_record_id = edit ? state->car_detail.id : 0;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter saves; Esc cancels without writing.");
}

static void open_driver_dialog(shell_state *const state, const bool edit, const char *const name,
                               const char *const short_name, const char *const notes)
{
    apl_tui_dialog_open_form(&state->dialog, edit ? "Edit driver" : "Create driver",
                             "Display name is required. Duplicate names are allowed as separate "
                             "identities.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Name", name, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Short name", short_name, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes", notes, false);
    state->dialog_context =
        edit ? SHELL_DIALOG_CONTEXT_DRIVER_EDIT : SHELL_DIALOG_CONTEXT_DRIVER_CREATE;
    state->dialog_record_id = edit ? state->driver_detail.id : 0;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter saves; Esc cancels without writing.");
}

static void open_membership_add_dialog(shell_state *const state, const int64_t driver_id,
                                       const char *const start_date, const char *const end_date,
                                       const char *const role, const char *const notes)
{
    if (active_team_id(state) == 0) {
        set_status_warning(state, "Select an active team before adding a membership.");
        return;
    }
    apl_tui_dialog_open_form(&state->dialog, "Add team membership",
                             "Start date is required. End date and role are optional and "
                             "user-entered.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Start date", start_date, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "End date", end_date, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Role/seat", role, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes", notes, false);
    state->dialog_context = SHELL_DIALOG_CONTEXT_MEMBERSHIP_ADD;
    state->dialog_driver_id = driver_id;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter adds the membership; Esc writes nothing.");
}

static void open_membership_end_dialog(shell_state *const state, const int64_t membership_id,
                                       const char *const start_date, const char *const end_date)
{
    apl_tui_dialog_open_form(&state->dialog, "End team membership",
                             "Enter the final date for this membership period. The period remains "
                             "in history.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Start date", start_date, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "End date", end_date, true);
    state->dialog_context = SHELL_DIALOG_CONTEXT_MEMBERSHIP_END;
    state->dialog_record_id = membership_id;
    state->dialog.fields[0].required = false;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter ends the period; Esc writes nothing.");
}

static bool perform_season_create(shell_state *const state, const char *const label,
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

static bool perform_season_update(shell_state *const state, const char *const label,
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

static void perform_season_archive(shell_state *const state, const int64_t id,
                                   apl_error *const error)
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

static bool perform_car_create(shell_state *const state, const char *const label,
                               const char *const chassis, const char *const notes,
                               apl_error *const error)
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

static bool perform_car_update(shell_state *const state, const char *const label,
                               const char *const chassis, const char *const notes,
                               apl_error *const error)
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

static void perform_car_archive(shell_state *const state, const int64_t id, apl_error *const error)
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

static bool perform_driver_create(shell_state *const state, const char *const name,
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

static bool perform_driver_update(shell_state *const state, const char *const name,
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

static void perform_driver_archive(shell_state *const state, const int64_t id,
                                   apl_error *const error)
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

static bool perform_membership_add(shell_state *const state, const char *const start_date,
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

static bool perform_membership_end(shell_state *const state, const char *const end_date,
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

static void free_picker_teams(shell_state *const state)
{
    apl_team_service_free_list(state->picker_teams);
    state->picker_teams = nullptr;
    state->picker_team_count = 0;
}

static void close_shell_dialog(shell_state *const state)
{
    apl_tui_dialog_init(&state->dialog);
    state->dialog_context = SHELL_DIALOG_CONTEXT_NONE;
    free_picker_teams(state);
    apl_tui_focus_pop(&state->focus);
}

/** Rebuild the choice dialog from the loaded picker teams. */
static void build_team_picker_dialog(shell_state *const state)
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
                       team->archived ? " [archived]" : "");
        const unsigned option =
            apl_tui_dialog_add_option(&state->dialog, label, !team->archived, active);
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

static apl_status load_picker_teams(shell_state *const state, apl_error *const error)
{
    free_picker_teams(state);
    return apl_team_service_list(state->app, state->picker_include_archived, &state->picker_teams,
                                 &state->picker_team_count, error);
}

static void open_team_picker(shell_state *const state)
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

static void rebuild_team_picker(shell_state *const state, const bool include_archived)
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

static void finish_team_picker(shell_state *const state, const apl_tui_dialog_result result)
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
        if (team.archived) {
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

static void open_person_detail(shell_state *const state, const apl_driver driver)
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

static void finish_driver_picker(shell_state *const state, const apl_tui_dialog_result result)
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

/** Present a driver choice for ambiguous names or a command without a target. */
static void open_driver_choice(shell_state *const state, apl_driver *const drivers,
                               const size_t count, const int mode)
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
            apl_tui_dialog_add_option(&state->dialog, label, !drivers[index].archived, false);
        if (option != UINT_MAX) {
            state->driver_choice_ids[state->driver_choice_count++] = drivers[index].id;
        }
    }
    state->dialog_context = SHELL_DIALOG_CONTEXT_DRIVER_PICKER;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Up/Down selects; Enter continues; Esc cancels.");
}

static void set_section(shell_state *const state, const apl_tui_section section)
{
    state->section = section;
    const size_t tab = apl_tui_section_tab_index(section);
    if (tab != SIZE_MAX) {
        apl_tui_list_select(&state->tabs, tab);
    }
    apl_error error;
    apl_error_init(&error);
    switch (section) {
    case APL_TUI_SECTION_TEAMS:
        if (refresh_teams(state, active_team_id(state), &error) != APL_OK) {
            show_error_dialog(state, &error);
        }
        state->screen = state->any_teams ? SHELL_SCREEN_TEAM_LIST : SHELL_SCREEN_WELCOME;
        break;
    case APL_TUI_SECTION_PEOPLE:
        state->screen = SHELL_SCREEN_PEOPLE_LIST;
        if (load_people(state, 0, &error) != APL_OK) {
            show_error_dialog(state, &error);
        }
        break;
    case APL_TUI_SECTION_HISTORY:
        state->screen = SHELL_SCREEN_HISTORY_LIST;
        if (load_history(state, &error) != APL_OK) {
            show_error_dialog(state, &error);
        }
        break;
    default:
        state->screen = SHELL_SCREEN_PLACEHOLDER;
        break;
    }
    sync_status_context(state);
}

static void switch_section(shell_state *const state, const bool forward)
{
    set_section(state, forward ? apl_tui_section_next(state->section)
                               : apl_tui_section_previous(state->section));
}

static void handle_menu_action(shell_state *const state, const apl_tui_menu_action action)
{
    switch (action) {
    case APL_TUI_MENU_ACTION_OVERVIEW_ACTIVE:
        set_section(state, APL_TUI_SECTION_OVERVIEW);
        set_status(state, "Active-team overview is not implemented in this phase.");
        apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_BODY);
        return;
    case APL_TUI_MENU_ACTION_OVERVIEW_ALL:
        set_section(state, APL_TUI_SECTION_OVERVIEW);
        set_status(state, "All-teams overview is not implemented in this phase.");
        apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_BODY);
        return;
    case APL_TUI_MENU_ACTION_OPEN_PEOPLE:
        state->people_all_teams = false;
        set_section(state, APL_TUI_SECTION_PEOPLE);
        set_status(state, "People: drivers associated with the active team.");
        return;
    case APL_TUI_MENU_ACTION_OPEN_SEASONS: {
        apl_error error;
        apl_error_init(&error);
        state->section = APL_TUI_SECTION_TEAMS;
        state->screen = SHELL_SCREEN_SEASON_LIST;
        if (load_seasons(state, 0, &error) != APL_OK) {
            show_error_dialog(state, &error);
        } else {
            sync_status_context(state);
            set_status(state, "Seasons/periods for the active team.");
        }
        return;
    }
    case APL_TUI_MENU_ACTION_OPEN_CARS: {
        apl_error error;
        apl_error_init(&error);
        state->section = APL_TUI_SECTION_TEAMS;
        state->screen = SHELL_SCREEN_CAR_LIST;
        if (load_cars(state, 0, &error) != APL_OK) {
            show_error_dialog(state, &error);
        } else {
            sync_status_context(state);
            set_status(state, "Cars/chassis for the active team.");
        }
        return;
    }
    case APL_TUI_MENU_ACTION_DATABASE_INFO:
        show_info_dialog(
            state, "Database information",
            "The SQLite database is chosen at launch with --database or the default data "
            "directory. Schema upgrades are applied by the migration runner; no settings are "
            "editable in this phase.");
        set_status(state, "Database information.");
        return;
    case APL_TUI_MENU_ACTION_HELP_KEYBOARD:
        show_info_dialog(state, "Keyboard and mouse help",
                         "F10 or m focuses the window bar. Tab cycles focus. , and . switch views. "
                         "Arrow keys move and Enter activates. Esc closes menus and dialogs. Mouse "
                         "clicks use the same actions as the keyboard. q quits.");
        return;
    case APL_TUI_MENU_ACTION_HELP_COMMANDS: {
        char help[APL_TUI_COMMAND_HELP_MAX];
        apl_tui_command_registry_help(help, sizeof(help));
        show_info_dialog(state, "Command help", help);
        return;
    }
    case APL_TUI_MENU_ACTION_ABOUT: {
        char version[128];
        apl1_version_info(version, sizeof(version));
        show_info_dialog(state, "About apl1",
                         "apl1 is a local, user-maintained Formula 1 team management database. "
                         "It does not generate schedules, results, or missing facts.");
        set_status(state, "%s", version);
        return;
    }
    case APL_TUI_MENU_ACTION_VERSION: {
        char version[128];
        apl1_version_info(version, sizeof(version));
        show_info_dialog(state, "Version information", version);
        return;
    }
    case APL_TUI_MENU_ACTION_NONE:
        break;
    }
}

static unsigned keys_button_start(const unsigned columns)
{
    constexpr unsigned KEY_LABEL_WIDTH = 6; /* "[Keys]" */
    return (columns > KEY_LABEL_WIDTH + 2) ? columns - KEY_LABEL_WIDTH - 1 : columns;
}

static void draw_window_bar(shell_state *const state)
{
    struct ncplane *const plane = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_WINDOW);
    const apl_tui_layout *const layout = apl_tui_frame_layout(&state->frame);
    if (plane == nullptr || layout == nullptr) {
        return;
    }
    apl_tui_theme_apply(&state->theme, plane, APL_TUI_ROLE_WINDOW_BAR);
    for (unsigned column = 0; column < layout->window.cols; ++column) {
        (void)ncplane_putchar_yx(plane, 0, (int)column, ' ');
    }
    for (size_t index = 0; index < APL_TUI_MENU_GROUP_COUNT; ++index) {
        const apl_tui_menu_group group = (apl_tui_menu_group)index;
        const bool focused =
            state->focus.region == APL_TUI_FOCUS_WINDOW &&
            (state->menu.open ? state->menu.group == group : state->menu.group_focus == group);
        apl_tui_theme_apply(&state->theme, plane,
                            focused ? APL_TUI_ROLE_WINDOW_FOCUS : APL_TUI_ROLE_WINDOW_BAR);
        apl_tui_frame_draw_text(plane, 0, (int)apl_tui_menu_heading_offset(group),
                                apl_tui_menu_group_label(group));
    }
    if (layout->window.cols > 48) {
        apl_tui_theme_apply(&state->theme, plane, APL_TUI_ROLE_WINDOW_FOCUS);
        apl_tui_frame_draw_text(plane, 0, (int)keys_button_start(layout->window.cols), "[Keys]");
    }
    apl_tui_theme_reset(plane);
}

static void draw_menu_popup(shell_state *const state)
{
    if (!state->menu.open) {
        return;
    }
    struct ncplane *const popup = apl_tui_frame_popup(&state->frame);
    if (popup == nullptr) {
        return;
    }
    unsigned rows = 0;
    unsigned cols = 0;
    ncplane_dim_yx(popup, &rows, &cols);
    if (rows < 3 || cols < 4) {
        return;
    }

    apl_tui_theme_apply(&state->theme, popup, APL_TUI_ROLE_MENU);
    for (unsigned y = 0; y < rows; ++y) {
        for (unsigned x = 0; x < cols; ++x) {
            (void)ncplane_putchar_yx(popup, (int)y, (int)x, ' ');
        }
    }

    for (unsigned x = 1; x + 1 < cols; ++x) {
        (void)ncplane_putchar_yx(popup, 0, (int)x, '-');
        (void)ncplane_putchar_yx(popup, (int)(rows - 1), (int)x, '-');
    }
    for (unsigned y = 1; y + 1 < rows; ++y) {
        (void)ncplane_putchar_yx(popup, (int)y, 0, '|');
        (void)ncplane_putchar_yx(popup, (int)y, (int)(cols - 1), '|');
    }
    (void)ncplane_putchar_yx(popup, 0, 0, '+');
    (void)ncplane_putchar_yx(popup, 0, (int)(cols - 1), '+');
    (void)ncplane_putchar_yx(popup, (int)(rows - 1), 0, '+');
    (void)ncplane_putchar_yx(popup, (int)(rows - 1), (int)(cols - 1), '+');

    const size_t count = apl_tui_menu_item_count(state->menu.group);
    for (size_t index = 0; index < count && index + 1 < rows - 1; ++index) {
        const apl_tui_menu_item *const item = apl_tui_menu_item_at(state->menu.group, index);
        const bool focused = index == state->menu.item_focus;
        char line[160];
        (void)snprintf(line, sizeof(line), "%c %s", focused ? '>' : ' ', item->label);
        apl_tui_theme_apply(&state->theme, popup,
                            !item->enabled
                                ? APL_TUI_ROLE_MENU_DISABLED
                                : (focused ? APL_TUI_ROLE_MENU_FOCUS : APL_TUI_ROLE_MENU));
        apl_tui_frame_draw_text(popup, (int)(index + 1), 1, line);
    }
    apl_tui_theme_reset(popup);
}

static void draw_tab_rail(shell_state *const state)
{
    struct ncplane *const rail = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_TABS);
    const apl_tui_layout *const layout = apl_tui_frame_layout(&state->frame);
    if (rail == nullptr || layout == nullptr) {
        return;
    }

    const bool compact = layout->mode == APL_TUI_LAYOUT_COMPACT;
    const unsigned content_rows = (layout->tabs.rows > layout->team_switcher.rows)
                                      ? layout->tabs.rows - layout->team_switcher.rows
                                      : layout->tabs.rows;
    apl_tui_list_set_viewport_rows(&state->tabs, content_rows);
    const size_t tab = apl_tui_section_tab_index(state->section);
    if (tab != SIZE_MAX) {
        apl_tui_list_select(&state->tabs, tab);
    }

    const size_t visible = apl_tui_list_visible_count(&state->tabs);
    for (size_t offset = 0; offset < visible && offset < content_rows; ++offset) {
        const size_t index = state->tabs.offset + offset;
        const apl_tui_section section = apl_tui_section_tab_at(index);
        if (section == APL_TUI_SECTION_COUNT) {
            break;
        }
        const bool active = section == state->section;
        const bool focused =
            state->focus.region == APL_TUI_FOCUS_TABS && index == state->tabs.selected;
        const char *const label =
            compact ? apl_tui_section_short_label(section) : apl_tui_section_label(section);
        char line[64];
        (void)snprintf(line, sizeof(line), "%c%c %s", active ? '*' : ' ', focused ? '>' : ' ',
                       label);
        apl_tui_theme_apply(&state->theme, rail,
                            focused
                                ? APL_TUI_ROLE_TAB_FOCUS
                                : (active ? APL_TUI_ROLE_TAB_ACTIVE : APL_TUI_ROLE_TAB_INACTIVE));
        apl_tui_frame_draw_line(rail, (int)offset, line);
    }
    apl_tui_theme_reset(rail);
}

static void draw_team_switcher(shell_state *const state)
{
    struct ncplane *const rail = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_TABS);
    const apl_tui_layout *const layout = apl_tui_frame_layout(&state->frame);
    if (rail == nullptr || layout == nullptr || layout->team_switcher.rows == 0) {
        return;
    }
    const unsigned start_row = layout->team_switcher.y - layout->tabs.y;
    const unsigned rows = layout->team_switcher.rows;
    const bool focused = state->focus.region == APL_TUI_FOCUS_TABS && state->team_focus;

    apl_tui_theme_apply(&state->theme, rail, focused ? APL_TUI_ROLE_TEAM_FOCUS : APL_TUI_ROLE_TEAM);
    if (rows >= 2) {
        apl_tui_frame_draw_line(rail, (int)start_row, "----------------------");
    }
    char line[APL_TUI_STATUS_TEAM_MAX + 16];
    (void)snprintf(line, sizeof(line), "%cTeam: %s", focused ? '>' : ' ',
                   active_status_has_team(state->active_status) ? state->active_team.name
                                                                : "(none)");
    apl_tui_frame_draw_line(rail, (int)(start_row + rows - 1), line);
    apl_tui_theme_reset(rail);
}

static void draw_welcome(shell_state *const state)
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

static void draw_team_list(shell_state *const state)
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
                       team->name, team->archived ? " [archived]" : "",
                       team->notes[0] != '\0' ? "  -  " : "", team->notes);
        apl_tui_theme_apply(&state->theme, body,
                            focused ? APL_TUI_ROLE_SELECTION : APL_TUI_ROLE_BODY);
        apl_tui_frame_draw_line(body, (int)offset, line);
        apl_tui_theme_reset(body);
    }
}

static void draw_team_detail(shell_state *const state)
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
                   state->detail.archived ? "Archived" : "Active");
    apl_tui_frame_draw_line(body, row++, line);

    apl_tui_frame_draw_line(body, row++, "Notes:");
    const char *const notes = state->detail.notes[0] != '\0' ? state->detail.notes : "not recorded";
    const unsigned note_rows =
        apl_tui_frame_draw_wrapped(body, row, 2, cols > 2 ? cols - 2 : cols, notes);
    row += (note_rows > 0) ? (int)note_rows : 1;

    ++row;
    (void)snprintf(line, sizeof(line), "Created: %s", state->detail.created_at);
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Updated: %s", state->detail.updated_at);
    apl_tui_frame_draw_line(body, row++, line);
    if (state->detail.archived) {
        (void)snprintf(line, sizeof(line), "Archived: %s", state->detail.archived_at);
        apl_tui_frame_draw_line(body, row++, line);
    }

    ++row;
    apl_tui_frame_draw_line(body, row++, "Related records");
    apl_tui_frame_draw_line(body, row++, "  People: press p");
    apl_tui_frame_draw_line(body, row++, "  Seasons/periods: press o");
    apl_tui_frame_draw_line(body, row++, "  Cars/chassis: press c");
    apl_tui_frame_draw_line(body, row++, "  Money: not implemented in this phase");
}

static void draw_season_list(shell_state *const state)
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
                       season->archived ? " [archived]" : "",
                       season->notes[0] != '\0' ? "  -  " : "", season->notes);
        apl_tui_theme_apply(&state->theme, body,
                            focused ? APL_TUI_ROLE_SELECTION : APL_TUI_ROLE_BODY);
        apl_tui_frame_draw_line(body, (int)offset, line);
        apl_tui_theme_reset(body);
    }
}

static void draw_season_detail(shell_state *const state)
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
    (void)snprintf(line, sizeof(line), "Status: %s", season->archived ? "Archived" : "Active");
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
    (void)snprintf(line, sizeof(line), "Created: %s", season->created_at);
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Updated: %s", season->updated_at);
    apl_tui_frame_draw_line(body, row++, line);
    if (season->archived) {
        (void)snprintf(line, sizeof(line), "Archived: %s", season->archived_at);
        apl_tui_frame_draw_line(body, row++, line);
    }
    ++row;
    apl_tui_frame_draw_line(body, row++, "e edit   x archive   s select as current view context");
    apl_tui_frame_draw_line(body, row++, "Esc back to seasons");
}

static void draw_car_list(shell_state *const state)
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
                       car->archived ? " [archived]" : "");
        apl_tui_theme_apply(&state->theme, body,
                            focused ? APL_TUI_ROLE_SELECTION : APL_TUI_ROLE_BODY);
        apl_tui_frame_draw_line(body, (int)offset, line);
        apl_tui_theme_reset(body);
    }
}

static void draw_car_detail(shell_state *const state)
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
    (void)snprintf(line, sizeof(line), "Status: %s", car->archived ? "Archived" : "Active");
    apl_tui_frame_draw_line(body, row++, line);
    apl_tui_frame_draw_line(body, row++, "Notes:");
    const unsigned note_rows =
        apl_tui_frame_draw_wrapped(body, row, 2, cols > 2 ? cols - 2 : cols,
                                   car->notes[0] != '\0' ? car->notes : "not recorded");
    row += (int)(note_rows > 0 ? note_rows : 1);
    ++row;
    (void)snprintf(line, sizeof(line), "Created: %s", car->created_at);
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Updated: %s", car->updated_at);
    apl_tui_frame_draw_line(body, row++, line);
    if (car->archived) {
        (void)snprintf(line, sizeof(line), "Archived: %s", car->archived_at);
        apl_tui_frame_draw_line(body, row++, line);
    }
    ++row;
    apl_tui_frame_draw_line(body, row++, "e edit   x archive   Esc back to cars");
}

static void draw_people_list(shell_state *const state)
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
                           driver->archived ? " [archived]" : "");
        } else {
            (void)snprintf(line, sizeof(line), "%c %.100s  [%s]%s", focused ? '>' : ' ',
                           driver->name, membership_state, driver->archived ? " [archived]" : "");
        }
        apl_tui_theme_apply(&state->theme, body,
                            focused ? APL_TUI_ROLE_SELECTION : APL_TUI_ROLE_BODY);
        apl_tui_frame_draw_line(body, (int)offset, line);
        apl_tui_theme_reset(body);
    }
}

static void draw_person_detail(shell_state *const state)
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
    (void)snprintf(line, sizeof(line), "Status: %s", driver->archived ? "Archived" : "Active");
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
    (void)snprintf(line, sizeof(line), "Created: %s", driver->created_at);
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Updated: %s", driver->updated_at);
    apl_tui_frame_draw_line(body, row++, line);
    ++row;
    apl_tui_frame_draw_line(body, row++, "m add membership   d end current membership");
    apl_tui_frame_draw_line(body, row++, "e edit driver   x archive   Esc back to people");
}

static const char *history_entity_filter_short_name(const unsigned index)
{
    static const char *const NAMES[] = {"all", "team", "season", "driver", "member", "car"};
    return (index < sizeof(NAMES) / sizeof(NAMES[0])) ? NAMES[index] : "all";
}

static const char *history_action_filter_short_name(const unsigned index)
{
    static const char *const NAMES[] = {
        "all",
        "team:created",
        "team:renamed",
        "team:archived",
        "team:selected",
        "season:created",
        "season:updated",
        "season:archived",
        "car:created",
        "car:updated",
        "car:archived",
        "driver:created",
        "driver:updated",
        "driver:archived",
        "member:added",
        "member:updated",
        "member:ended",
    };
    return (index < sizeof(NAMES) / sizeof(NAMES[0])) ? NAMES[index] : "all";
}

static const char *history_date_filter_short_name(const unsigned preset)
{
    switch (preset) {
    case 1:
        return "7d";
    case 2:
        return "30d";
    case 3:
        return "365d";
    default:
        return "all";
    }
}

static void append_history_filter_button(char *const out, const size_t out_size,
                                         unsigned *const cursor, unsigned starts[5],
                                         unsigned ends[5], const unsigned index,
                                         const char *const label)
{
    starts[index] = *cursor;
    (void)apl_tui_text_append(out, out_size - *cursor, label);
    *cursor = (unsigned)strlen(out);
    ends[index] = *cursor;
    (void)apl_tui_text_append(out, out_size - *cursor, "  ");
    *cursor = (unsigned)strlen(out);
}

static void build_history_filter_bar(const shell_state *const state, char *const out,
                                     const size_t out_size, unsigned starts[5], unsigned ends[5])
{
    out[0] = '\0';
    unsigned cursor = 0;
    char label[96];
    (void)snprintf(label, sizeof(label), "Team:%s", state->history_all_teams ? "all" : "active");
    append_history_filter_button(out, out_size, &cursor, starts, ends, 0, label);
    (void)snprintf(label, sizeof(label), "Ent:%s",
                   history_entity_filter_short_name(state->history_entity_filter));
    append_history_filter_button(out, out_size, &cursor, starts, ends, 1, label);
    (void)snprintf(label, sizeof(label), "Act:%s",
                   history_action_filter_short_name(state->history_action_filter));
    append_history_filter_button(out, out_size, &cursor, starts, ends, 2, label);
    (void)snprintf(label, sizeof(label), "Date:%s",
                   history_date_filter_short_name(state->history_date_preset));
    append_history_filter_button(out, out_size, &cursor, starts, ends, 3, label);
    append_history_filter_button(out, out_size, &cursor, starts, ends, 4, "Clear");
}

static void draw_history_list(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    const char *const scope = state->history_all_teams ? "all teams" : "active team";
    char header[256];
    (void)snprintf(header, sizeof(header), "History: %s (%zu events)", scope, state->event_count);
    apl_tui_frame_draw_line(body, 0, header);

    unsigned starts[5] = {};
    unsigned ends[5] = {};
    char filters[512];
    build_history_filter_bar(state, filters, sizeof(filters), starts, ends);
    apl_tui_theme_apply(&state->theme, body, APL_TUI_ROLE_BODY);
    apl_tui_frame_draw_line(body, 1, filters);
    apl_tui_theme_reset(body);

    const unsigned rows = apl_tui_frame_layout(&state->frame)->body.rows;
    if (state->event_count == 0) {
        apl_tui_frame_draw_line(body, 3, "No activity matches the current filters.");
        return;
    }
    const size_t visible = apl_tui_list_visible_count(&state->list);
    for (size_t offset = 0; offset < visible && offset + 3 < rows - 1; ++offset) {
        const size_t index = state->list.offset + offset;
        const apl_activity_event *const event = &state->events[index];
        const bool focused = index == state->list.selected;
        char stamp[17];
        (void)snprintf(stamp, sizeof(stamp), "%.16s", event->occurred_at);
        char line[APL_ACTIVITY_SUMMARY_MAX + 160];
        (void)snprintf(line, sizeof(line), "%c %s  %-16s %s", focused ? '>' : ' ', stamp,
                       event->action, event->summary);
        apl_tui_theme_apply(&state->theme, body,
                            focused ? APL_TUI_ROLE_SELECTION : APL_TUI_ROLE_BODY);
        apl_tui_frame_draw_line(body, (int)(offset + 3), line);
        apl_tui_theme_reset(body);
    }
}

static const char *context_key_hint(const shell_state *const state)
{
    if (state->section == APL_TUI_SECTION_PEOPLE) {
        return (state->screen == SHELL_SCREEN_PERSON_DETAIL)
                   ? "m membership | d end | e edit | x archive | Esc back"
                   : "n new | Enter open | m membership | a archived | A all teams";
    }
    if (state->section == APL_TUI_SECTION_HISTORY) {
        return "a scope | f entity | g action | d dates | c clear | Enter detail";
    }
    if (state->section != APL_TUI_SECTION_TEAMS) {
        return ", . views | : commands | m menu | q quit";
    }
    switch (state->screen) {
    case SHELL_SCREEN_WELCOME:
        return "n new team | q quit | : commands";
    case SHELL_SCREEN_TEAM_LIST:
        return "n new | Enter open | s switch | a archived";
    case SHELL_SCREEN_TEAM_DETAIL:
        return "e edit | x archive | o seasons | c cars | p people | s switch";
    case SHELL_SCREEN_SEASON_LIST:
        return "n new | Enter open | a archived | Esc team";
    case SHELL_SCREEN_SEASON_DETAIL:
        return "e edit | x archive | s select | Esc seasons";
    case SHELL_SCREEN_CAR_LIST:
        return "n new | Enter open | a archived | Esc team";
    case SHELL_SCREEN_CAR_DETAIL:
        return "e edit | x archive | Esc cars";
    case SHELL_SCREEN_PLACEHOLDER:
    case SHELL_SCREEN_PEOPLE_LIST:
    case SHELL_SCREEN_PERSON_DETAIL:
    case SHELL_SCREEN_HISTORY_LIST:
        break;
    }
    return ", . views | : commands | m menu | q quit";
}

static void draw_key_hints(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    const apl_tui_layout *const layout = apl_tui_frame_layout(&state->frame);
    if (body == nullptr || layout == nullptr || layout->body.rows == 0) {
        return;
    }
    char line[320];
    (void)snprintf(line, sizeof(line), "Keys: %s", context_key_hint(state));
    apl_tui_theme_apply(&state->theme, body, APL_TUI_ROLE_MUTED);
    apl_tui_frame_draw_line(body, (int)(layout->body.rows - 1), line);
    apl_tui_theme_reset(body);
}

static void draw_placeholder(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    char line[128];
    (void)snprintf(line, sizeof(line), "%s is not implemented in this phase.",
                   apl_tui_section_label(state->section));
    apl_tui_frame_draw_line(body, 1, line);
    apl_tui_frame_draw_line(body, 3, "Use the tab rail or , and . to switch views.");
    apl_tui_frame_draw_line(body, 5, "No records are generated for this view.");
}

/** Draw dialog text while preserving explicit newlines and blank lines.
 *
 * The regular word wrapper treats newlines as spaces, which turns command help
 * into an unreadable paragraph. Each logical line is wrapped independently so
 * headings and grouped entries stay on their own rows. */
static int draw_dialog_block(struct ncplane *const plane, const int row, const int col,
                             const unsigned width, const char *const text)
{
    if (plane == nullptr || text == nullptr || width == 0) {
        return 0;
    }
    int current = row;
    const char *cursor = text;
    while (*cursor != '\0') {
        const char *const newline = strchr(cursor, '\n');
        size_t length = (newline != nullptr) ? (size_t)(newline - cursor) : strlen(cursor);
        if (length > 0) {
            char line[APL_TUI_DIALOG_MESSAGE_MAX];
            if (length >= sizeof(line)) {
                length = sizeof(line) - 1;
            }
            memcpy(line, cursor, length);
            line[length] = '\0';
            const unsigned used = apl_tui_frame_draw_wrapped(plane, current, col, width, line);
            current += (used > 0) ? (int)used : 1;
        } else {
            ++current;
        }
        if (newline == nullptr) {
            break;
        }
        cursor = newline + 1;
    }
    return current - row;
}

static void draw_dialog(shell_state *const state)
{
    struct ncplane *const overlay = apl_tui_frame_overlay(&state->frame);
    if (overlay == nullptr) {
        return;
    }
    unsigned rows = 0;
    unsigned cols = 0;
    ncplane_dim_yx(overlay, &rows, &cols);
    if (rows < 3 || cols < 4) {
        return;
    }

    apl_tui_theme_apply(&state->theme, overlay, APL_TUI_ROLE_DIALOG);
    for (unsigned y = 0; y < rows; ++y) {
        for (unsigned x = 0; x < cols; ++x) {
            (void)ncplane_putchar_yx(overlay, (int)y, (int)x, ' ');
        }
    }

    apl_tui_theme_apply(&state->theme, overlay, APL_TUI_ROLE_DIALOG_TITLE);
    apl_tui_frame_draw_text(overlay, 1, 2, state->dialog.title);
    apl_tui_theme_apply(&state->theme, overlay, APL_TUI_ROLE_DIALOG);
    if (state->dialog.kind == APL_TUI_DIALOG_FORM || state->dialog.kind == APL_TUI_DIALOG_CHOICE) {
        apl_tui_frame_draw_text(overlay, 2, 2, state->dialog.message);
    }

    const int button_row = (int)rows - 2;
    switch (state->dialog.kind) {
    case APL_TUI_DIALOG_FORM: {
        for (unsigned index = 0; index < state->dialog.field_count; ++index) {
            const unsigned row = APL_TUI_DIALOG_CONTENT_ROW + index;
            if (row >= (unsigned)button_row) {
                break;
            }
            const apl_tui_dialog_field *const field = &state->dialog.fields[index];
            const bool focused = state->dialog.focus == index;
            char line[APL_TUI_DIALOG_FIELD_LABEL_MAX + APL_TUI_DIALOG_FIELD_VALUE_MAX + 24];
            (void)snprintf(line, sizeof(line), "%c %s: %s%s%s", focused ? '>' : ' ', field->label,
                           field->value, focused ? "\xe2\x96\x88" : "",
                           field->invalid ? "  !" : "");
            apl_tui_theme_apply(&state->theme, overlay,
                                field->invalid
                                    ? APL_TUI_ROLE_ERROR
                                    : (focused ? APL_TUI_ROLE_SELECTION : APL_TUI_ROLE_BODY));
            apl_tui_frame_draw_text(overlay, (int)row, 2, line);
        }
        if (rows > 4 && state->dialog.validation[0] != '\0') {
            apl_tui_theme_apply(&state->theme, overlay, APL_TUI_ROLE_ERROR);
            apl_tui_frame_draw_text(overlay, (int)rows - 3, 2, state->dialog.validation);
        }
        apl_tui_theme_apply(&state->theme, overlay,
                            state->dialog.focus == state->dialog.field_count
                                ? APL_TUI_ROLE_SELECTION
                                : APL_TUI_ROLE_DIALOG);
        apl_tui_frame_draw_text(overlay, button_row, 2, "[ Save ]");
        apl_tui_theme_apply(&state->theme, overlay,
                            state->dialog.focus == state->dialog.field_count + 1
                                ? APL_TUI_ROLE_SELECTION
                                : APL_TUI_ROLE_DIALOG);
        apl_tui_frame_draw_text(overlay, button_row, 14, "[ Cancel ]");
        break;
    }
    case APL_TUI_DIALOG_CHOICE: {
        const unsigned visible = apl_tui_dialog_visible_options(&state->dialog, rows);
        apl_tui_dialog_set_viewport_rows(&state->dialog, visible);
        for (unsigned offset = 0; offset < visible; ++offset) {
            const unsigned index = state->dialog.option_offset + offset;
            const apl_tui_dialog_option *const option =
                apl_tui_dialog_option_at(&state->dialog, index);
            if (option == nullptr) {
                break;
            }
            const bool focused = state->dialog.focus == index;
            char line[APL_TUI_DIALOG_OPTION_LABEL_MAX + 8];
            (void)snprintf(line, sizeof(line), "%c%c %s", focused ? '>' : ' ',
                           option->active ? '*' : ' ', option->label);
            apl_tui_theme_apply(&state->theme, overlay,
                                !option->enabled
                                    ? APL_TUI_ROLE_MENU_DISABLED
                                    : (focused ? APL_TUI_ROLE_SELECTION : APL_TUI_ROLE_BODY));
            apl_tui_frame_draw_text(overlay, (int)(APL_TUI_DIALOG_CONTENT_ROW + offset), 2, line);
        }
        apl_tui_theme_apply(&state->theme, overlay, APL_TUI_ROLE_MUTED);
        apl_tui_frame_draw_text(overlay, button_row, 2,
                                "Up/Down select   Enter choose   Esc cancel");
        break;
    }
    case APL_TUI_DIALOG_CONFIRM: {
        (void)draw_dialog_block(overlay, 2, 2, cols > 4 ? cols - 4 : cols, state->dialog.message);
        apl_tui_theme_apply(&state->theme, overlay,
                            state->dialog.focus == 0 ? APL_TUI_ROLE_SELECTION
                                                     : APL_TUI_ROLE_DIALOG);
        apl_tui_frame_draw_text(overlay, button_row, 2, "[ Cancel ]");
        apl_tui_theme_apply(&state->theme, overlay,
                            state->dialog.focus == 1 ? APL_TUI_ROLE_SELECTION
                                                     : APL_TUI_ROLE_DIALOG);
        apl_tui_frame_draw_text(overlay, button_row, 15, "[ Confirm ]");
        break;
    }
    case APL_TUI_DIALOG_ERROR: {
        (void)draw_dialog_block(overlay, 2, 2, cols > 4 ? cols - 4 : cols, state->dialog.message);
        apl_tui_theme_apply(&state->theme, overlay, APL_TUI_ROLE_SELECTION);
        apl_tui_frame_draw_text(overlay, button_row, 2, "[ OK ]");
        break;
    }
    case APL_TUI_DIALOG_NONE:
        break;
    }

    /* Draw the frame last so long values can never overwrite its edges. */
    apl_tui_theme_apply(&state->theme, overlay, APL_TUI_ROLE_DIALOG);
    for (unsigned x = 1; x + 1 < cols; ++x) {
        (void)ncplane_putchar_yx(overlay, 0, (int)x, '-');
        (void)ncplane_putchar_yx(overlay, (int)(rows - 1), (int)x, '-');
    }
    for (unsigned y = 1; y + 1 < rows; ++y) {
        (void)ncplane_putchar_yx(overlay, (int)y, 0, '|');
        (void)ncplane_putchar_yx(overlay, (int)y, (int)(cols - 1), '|');
    }
    (void)ncplane_putchar_yx(overlay, 0, 0, '+');
    (void)ncplane_putchar_yx(overlay, 0, (int)(cols - 1), '+');
    (void)ncplane_putchar_yx(overlay, (int)(rows - 1), 0, '+');
    (void)ncplane_putchar_yx(overlay, (int)(rows - 1), (int)(cols - 1), '+');

    apl_tui_theme_reset(overlay);
}

static void draw_screen(shell_state *const state)
{
    draw_window_bar(state);
    draw_tab_rail(state);
    draw_team_switcher(state);

    if (state->section == APL_TUI_SECTION_PEOPLE) {
        if (state->screen == SHELL_SCREEN_PERSON_DETAIL) {
            draw_person_detail(state);
        } else {
            draw_people_list(state);
        }
    } else if (state->section == APL_TUI_SECTION_HISTORY) {
        draw_history_list(state);
    } else if (state->section != APL_TUI_SECTION_TEAMS) {
        draw_placeholder(state);
    } else {
        switch (state->screen) {
        case SHELL_SCREEN_WELCOME:
            draw_welcome(state);
            break;
        case SHELL_SCREEN_TEAM_LIST:
            draw_team_list(state);
            break;
        case SHELL_SCREEN_TEAM_DETAIL:
            draw_team_detail(state);
            break;
        case SHELL_SCREEN_SEASON_LIST:
            draw_season_list(state);
            break;
        case SHELL_SCREEN_SEASON_DETAIL:
            draw_season_detail(state);
            break;
        case SHELL_SCREEN_CAR_LIST:
            draw_car_list(state);
            break;
        case SHELL_SCREEN_CAR_DETAIL:
            draw_car_detail(state);
            break;
        case SHELL_SCREEN_PLACEHOLDER:
        case SHELL_SCREEN_PEOPLE_LIST:
        case SHELL_SCREEN_PERSON_DETAIL:
        case SHELL_SCREEN_HISTORY_LIST:
            draw_placeholder(state);
            break;
        }
    }

    draw_key_hints(state);

    struct ncplane *const status = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_STATUS);
    const apl_tui_layout *const layout = apl_tui_frame_layout(&state->frame);
    if (status != nullptr && layout != nullptr) {
        apl_tui_style_role role = APL_TUI_ROLE_BODY;
        char line[768];
        if (apl_tui_command_line_is_open(&state->command_line)) {
            role = APL_TUI_ROLE_SELECTION;
            apl_tui_theme_apply(&state->theme, status, role);
            for (unsigned column = 0; column < layout->status.cols; ++column) {
                (void)ncplane_putchar_yx(status, 0, (int)column, ' ');
            }
            (void)apl_tui_command_line_render(&state->command_line, layout->status.cols, line,
                                              sizeof(line));
            apl_tui_frame_draw_line(status, 0, line);
            apl_tui_theme_reset(status);
        } else {
            if (state->status.message[0] != '\0') {
                switch (state->status.level) {
                case APL_TUI_STATUS_SUCCESS:
                    role = APL_TUI_ROLE_SUCCESS;
                    break;
                case APL_TUI_STATUS_WARNING:
                    role = APL_TUI_ROLE_WARNING;
                    break;
                case APL_TUI_STATUS_ERROR:
                    role = APL_TUI_ROLE_ERROR;
                    break;
                case APL_TUI_STATUS_INFO:
                    break;
                }
            }
            apl_tui_theme_apply(&state->theme, status, role);
            for (unsigned column = 0; column < layout->status.cols; ++column) {
                (void)ncplane_putchar_yx(status, 0, (int)column, ' ');
            }
            (void)apl_tui_status_render(&state->status, layout->status.cols, line, sizeof(line));
            apl_tui_frame_draw_line(status, 0, line);
            apl_tui_theme_reset(status);
        }
    }

    draw_menu_popup(state);

    if (apl_tui_dialog_active(&state->dialog)) {
        draw_dialog(state);
    }
}

static bool is_enter_key(const uint32_t id)
{
    return id == NCKEY_ENTER || id == '\n' || id == '\r';
}

static void update_command_suggestion(shell_state *const state)
{
    char suggestion[APL_TUI_COMMAND_SUGGESTION_MAX];
    apl_tui_command_complete(state->command_line.text, suggestion, sizeof(suggestion));
    if (strcmp(suggestion, state->command_line.text) == 0) {
        suggestion[0] = '\0';
    }
    apl_tui_command_line_set_suggestion(&state->command_line, suggestion);
}

static bool command_suggestion_applicable(const shell_state *const state)
{
    const char *const suggestion = state->command_line.suggestion;
    return suggestion[0] != '\0' && strcmp(suggestion, state->command_line.text) != 0 &&
           strstr(suggestion, " | ") == nullptr && strchr(suggestion, '<') == nullptr &&
           strchr(suggestion, '[') == nullptr;
}

static void open_command_line(shell_state *const state)
{
    if (apl_tui_dialog_active(&state->dialog)) {
        return;
    }
    apl_tui_command_line_open(&state->command_line);
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_COMMAND);
    update_command_suggestion(state);
    set_status(state, "Enter runs the command; Esc cancels. Command history is session-local.");
}

static void close_command_line(shell_state *const state)
{
    apl_tui_command_line_close(&state->command_line);
    apl_tui_focus_pop(&state->focus);
}

static bool text_is_numeric(const char *const text)
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

static void copy_pending_membership_values(shell_state *const state,
                                           const apl_tui_action *const action)
{
    (void)snprintf(state->pending_start, sizeof(state->pending_start), "%s", action->start);
    (void)snprintf(state->pending_end, sizeof(state->pending_end), "%s", action->end);
    (void)snprintf(state->pending_role, sizeof(state->pending_role), "%s", action->role);
    (void)snprintf(state->pending_notes, sizeof(state->pending_notes), "%s", action->notes);
}

static bool command_open_membership_end(shell_state *const state, const int64_t driver_id,
                                        const char *const proposed_end, apl_error *const error)
{
    apl_membership *memberships = nullptr;
    size_t count = 0;
    if (apl_membership_service_list_for_driver(state->app, driver_id, &memberships, &count,
                                               error) != APL_OK) {
        return false;
    }
    int64_t membership_id = 0;
    char start_date[APL_REFERENCE_DATE_MAX] = "";
    for (size_t index = count; index > 0; --index) {
        if (memberships[index - 1].end_date[0] == '\0') {
            membership_id = memberships[index - 1].id;
            (void)snprintf(start_date, sizeof(start_date), "%s", memberships[index - 1].start_date);
            break;
        }
    }
    apl_membership_service_free_list(memberships);
    if (membership_id == 0) {
        apl_error_set(error, APL_ERR_NOT_FOUND,
                      "that driver has no current membership period to end");
        return false;
    }
    open_membership_end_dialog(state, membership_id, start_date, proposed_end);
    return true;
}

/** Resolve a driver by id/name/unique prefix. Returns true when the command was
 *  handled (detail/form opened or an explicit choice presented). */
static bool dispatch_driver_target(shell_state *const state, const char *const target,
                                   const int mode, const apl_tui_action *const action,
                                   apl_error *const error)
{
    apl_driver *drivers = nullptr;
    size_t count = 0;
    if (apl_driver_service_list(state->app, true, &drivers, &count, error) != APL_OK) {
        return false;
    }
    const bool numeric = text_is_numeric(target);
    const int64_t target_id = numeric ? strtoll(target, nullptr, 10) : 0;
    size_t matches = 0;
    size_t first_exact = SIZE_MAX;
    size_t first_prefix = SIZE_MAX;
    for (size_t index = 0; index < count; ++index) {
        if (numeric) {
            if (drivers[index].id == target_id) {
                matches = 1;
                first_exact = index;
                break;
            }
            continue;
        }
        if (apl_tui_text_equal_ci(drivers[index].name, target)) {
            if (first_exact == SIZE_MAX) {
                first_exact = index;
            }
            ++matches;
        } else if (apl_tui_text_starts_with_ci(drivers[index].name, target)) {
            if (first_prefix == SIZE_MAX) {
                first_prefix = index;
            }
        }
    }
    size_t chosen = SIZE_MAX;
    if (matches == 1) {
        chosen = first_exact;
    } else if (!numeric && matches == 0 && first_prefix != SIZE_MAX) {
        size_t prefix_count = 0;
        for (size_t index = 0; index < count; ++index) {
            if (apl_tui_text_starts_with_ci(drivers[index].name, target) &&
                !apl_tui_text_equal_ci(drivers[index].name, target)) {
                ++prefix_count;
                chosen = index;
            }
        }
        if (prefix_count > 1) {
            chosen = SIZE_MAX;
        }
    }

    if (chosen != SIZE_MAX) {
        const int64_t driver_id = drivers[chosen].id;
        apl_driver_service_free_list(drivers);
        if (mode == 1 || mode == 3) {
            copy_pending_membership_values(state, action);
            if (mode == 1) {
                open_membership_add_dialog(state, driver_id, state->pending_start,
                                           state->pending_end, state->pending_role,
                                           state->pending_notes);
                return true;
            }
            state->dialog_driver_id = driver_id;
            state->section = APL_TUI_SECTION_PEOPLE;
            if (!perform_membership_add(state, action->start, action->end, action->role,
                                        action->notes, error)) {
                return false;
            }
            return true;
        }
        if (mode == 2 || mode == 4) {
            copy_pending_membership_values(state, action);
            if (mode == 2) {
                return command_open_membership_end(state, driver_id, state->pending_end, error);
            }
            apl_driver driver = {};
            if (apl_driver_service_get(state->app, driver_id, &driver, error) != APL_OK) {
                return false;
            }
            state->driver_detail = driver;
            state->section = APL_TUI_SECTION_PEOPLE;
            apl_membership *memberships = nullptr;
            size_t membership_count = 0;
            if (apl_membership_service_list_for_driver(state->app, driver_id, &memberships,
                                                       &membership_count, error) != APL_OK) {
                return false;
            }
            int64_t membership_id = 0;
            for (size_t index = membership_count; index > 0; --index) {
                if (memberships[index - 1].end_date[0] == '\0') {
                    membership_id = memberships[index - 1].id;
                    break;
                }
            }
            apl_membership_service_free_list(memberships);
            if (membership_id == 0) {
                apl_error_set(error, APL_ERR_NOT_FOUND,
                              "that driver has no current membership period to end");
                return false;
            }
            state->dialog_record_id = membership_id;
            return perform_membership_end(state, action->end, error);
        }
        apl_driver driver = {};
        if (apl_driver_service_get(state->app, driver_id, &driver, error) != APL_OK) {
            return false;
        }
        open_person_detail(state, driver);
        return true;
    }

    if (matches > 1 || (!numeric && first_prefix != SIZE_MAX)) {
        copy_pending_membership_values(state, action);
        open_driver_choice(state, drivers, count, (mode == 3) ? 1 : (mode == 4) ? 2 : mode);
        apl_driver_service_free_list(drivers);
        return true;
    }

    apl_driver_service_free_list(drivers);
    apl_error_set(error, APL_ERR_NOT_FOUND, "no driver matches '%s'", target);
    return false;
}

static bool dispatch_season_select(shell_state *const state, const char *const target,
                                   apl_error *const error)
{
    char normalized[APL_REFERENCE_LABEL_MAX];
    apl_status status = apl_reference_optional_normalize(target, "season target", normalized,
                                                         sizeof(normalized), error);
    if (status != APL_OK) {
        return false;
    }
    const int64_t team_id = active_team_id(state);
    apl_season *seasons = nullptr;
    size_t count = 0;
    if (apl_season_service_list(state->app, team_id, true, &seasons, &count, error) != APL_OK) {
        return false;
    }
    const bool numeric = text_is_numeric(normalized);
    const int64_t target_id = numeric ? strtoll(normalized, nullptr, 10) : 0;
    size_t matches = 0;
    size_t chosen = SIZE_MAX;
    for (size_t index = 0; index < count; ++index) {
        bool match = numeric ? seasons[index].id == target_id
                             : apl_tui_text_equal_ci(seasons[index].label, normalized);
        if (!match && !numeric) {
            match = apl_tui_text_starts_with_ci(seasons[index].label, normalized);
        }
        if (match) {
            ++matches;
            chosen = index;
        }
    }
    if (matches != 1) {
        apl_season_service_free_list(seasons);
        if (matches == 0) {
            apl_error_set(error, APL_ERR_NOT_FOUND, "no season matches '%s'; use its label or id",
                          normalized);
        } else {
            apl_error_set(error, APL_ERR_NOT_FOUND, "multiple seasons match '%s'; use the id",
                          normalized);
        }
        return false;
    }
    const apl_season selected = seasons[chosen];
    apl_season_service_free_list(seasons);
    state->selected_season_id = selected.id;
    state->season_detail = selected;
    state->section = APL_TUI_SECTION_TEAMS;
    state->screen = SHELL_SCREEN_SEASON_DETAIL;
    sync_status_context(state);
    set_status_success(state, "Current season context: %s.", selected.label);
    return true;
}

static bool dispatch_car_show(shell_state *const state, const char *const target,
                              apl_error *const error)
{
    const int64_t team_id = active_team_id(state);
    apl_car *cars = nullptr;
    size_t count = 0;
    if (apl_car_service_list(state->app, team_id, true, &cars, &count, error) != APL_OK) {
        return false;
    }
    const bool numeric = text_is_numeric(target);
    const int64_t target_id = numeric ? strtoll(target, nullptr, 10) : 0;
    size_t matches = 0;
    apl_car chosen = {};
    for (size_t index = 0; index < count; ++index) {
        if ((numeric && cars[index].id == target_id) ||
            (!numeric && apl_tui_text_equal_ci(cars[index].label, target))) {
            chosen = cars[index];
            ++matches;
        }
    }
    apl_car_service_free_list(cars);
    if (matches != 1) {
        apl_error_set(error, APL_ERR_NOT_FOUND, "no unique car matches '%s'", target);
        return false;
    }
    state->car_detail = chosen;
    state->section = APL_TUI_SECTION_TEAMS;
    state->screen = SHELL_SCREEN_CAR_DETAIL;
    sync_status_context(state);
    set_status(state, "Car %s. e edit, x archive, Esc back.", chosen.label);
    return true;
}

static void execute_command_line(shell_state *const state)
{
    if (apl_tui_command_line_empty(&state->command_line)) {
        close_command_line(state);
        return;
    }

    char line[APL_TUI_COMMAND_LINE_TEXT_MAX];
    (void)snprintf(line, sizeof(line), "%s", state->command_line.text);
    apl_tui_command_line_add_history(&state->command_line, line);
    close_command_line(state);

    const apl_tui_command_parse_result parsed = apl_tui_command_parse(line);
    if (parsed.status == APL_TUI_COMMAND_EMPTY) {
        return;
    }

    if (parsed.status == APL_TUI_COMMAND_INCOMPLETE && parsed.spec != nullptr &&
        parsed.spec->id == APL_TUI_COMMAND_TEAM_CREATE) {
        const char *const notes = apl_tui_command_option_value(&parsed.request, "notes");
        open_create_dialog(state, "", (notes != nullptr) ? notes : "");
        set_status(state, "Team name is missing; complete the form.");
        return;
    }
    if (parsed.status == APL_TUI_COMMAND_INCOMPLETE && parsed.spec != nullptr &&
        parsed.spec->id == APL_TUI_COMMAND_TEAM_SWITCH) {
        open_team_picker(state);
        return;
    }
    if (parsed.status != APL_TUI_COMMAND_OK) {
        char message[APL_TUI_COMMAND_ERROR_MAX + APL_TUI_COMMAND_SUGGESTION_MAX + 4];
        (void)snprintf(message, sizeof(message), "%s %s", parsed.error, parsed.suggestion);
        set_status_error(state, "%s", message);
        show_info_dialog(state, "Command error", message);
        return;
    }

    apl_error error;
    apl_error_init(&error);
    apl_tui_action action = {};
    if (apl_tui_action_from_request(&parsed.request, &action, &error) != APL_OK) {
        set_status_from_error(state, "Command failed", &error);
        show_info_dialog(state, "Command error", apl_error_message(&error));
        return;
    }

    switch (action.kind) {
    case APL_TUI_ACTION_SWITCH_SECTION:
        set_section(state, action.section);
        set_status(state, "View: %s", apl_tui_section_label(action.section));
        apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_BODY);
        return;
    case APL_TUI_ACTION_OVERVIEW_ALL:
        set_section(state, APL_TUI_SECTION_OVERVIEW);
        set_status(state, "All-teams overview is not implemented in this phase.");
        apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_BODY);
        return;
    case APL_TUI_ACTION_OPEN_TEAM_LIST:
        set_section(state, APL_TUI_SECTION_TEAMS);
        set_status(state, "Teams");
        apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_BODY);
        return;
    case APL_TUI_ACTION_OPEN_TEAM_PICKER:
        open_team_picker(state);
        return;
    case APL_TUI_ACTION_OPEN_TEAM_CREATE_FORM:
        open_create_form(state);
        return;
    case APL_TUI_ACTION_CREATE_TEAM_DIRECT:
        if (!perform_create(state, action.value, action.notes, &error)) {
            show_error_dialog(state, &error);
        }
        return;
    case APL_TUI_ACTION_TEAM_SWITCH_TARGET: {
        apl_team *teams = nullptr;
        size_t count = 0;
        if (apl_team_service_list(state->app, true, &teams, &count, &error) != APL_OK) {
            show_error_dialog(state, &error);
            return;
        }
        bool numeric = action.value[0] != '\0';
        for (const char *cursor = action.value; *cursor != '\0'; ++cursor) {
            if (*cursor < '0' || *cursor > '9') {
                numeric = false;
                break;
            }
        }
        const int64_t target_id = numeric ? strtoll(action.value, nullptr, 10) : 0;

        apl_team chosen = {};
        bool found = false;
        unsigned prefix_matches = 0;
        for (size_t index = 0; index < count; ++index) {
            if (numeric && teams[index].id == target_id) {
                chosen = teams[index];
                found = true;
                break;
            }
            if (!numeric && apl_tui_text_equal_ci(teams[index].name, action.value)) {
                chosen = teams[index];
                found = true;
                break;
            }
            if (!numeric && apl_tui_text_starts_with_ci(teams[index].name, action.value)) {
                ++prefix_matches;
                chosen = teams[index];
            }
        }
        if (!found && !numeric && prefix_matches == 1) {
            found = true;
        } else if (!found && !numeric && prefix_matches > 1) {
            apl_team_service_free_list(teams);
            set_status_warning(state, "Multiple teams match '%s'; choose one.", action.value);
            open_team_picker(state);
            return;
        }
        apl_team_service_free_list(teams);

        if (!found) {
            set_status_error(state, "No team matches '%s'.", action.value);
            char message[APL_TUI_COMMAND_ERROR_MAX];
            (void)snprintf(message, sizeof(message), "No team matches '%.180s'.", action.value);
            show_info_dialog(state, "Team not found", message);
            return;
        }
        if (chosen.archived) {
            set_status_warning(state, "Archived team '%s' cannot be active.", chosen.name);
            show_info_dialog(state, "Archived team",
                             "Archived teams cannot be the active workspace.");
            return;
        }
        if (active_status_has_team(state->active_status) && state->active_team.id == chosen.id) {
            set_status(state, "Team '%s' is already active.", chosen.name);
            return;
        }
        perform_set_active(state, chosen.id, &error);
        if (!apl_error_is_success(&error)) {
            show_error_dialog(state, &error);
        }
        return;
    }
    case APL_TUI_ACTION_OPEN_DRIVER_LIST:
        state->people_all_teams = false;
        set_section(state, APL_TUI_SECTION_PEOPLE);
        state->screen = SHELL_SCREEN_PEOPLE_LIST;
        set_status(state, "People: drivers associated with the active team.");
        return;
    case APL_TUI_ACTION_OPEN_DRIVER_CREATE_FORM:
        open_driver_dialog(state, false, action.value, action.notes, action.role);
        return;
    case APL_TUI_ACTION_CREATE_DRIVER_DIRECT:
        if (!perform_driver_create(state, action.value, action.notes, action.role, &error)) {
            show_error_dialog(state, &error);
        }
        return;
    case APL_TUI_ACTION_OPEN_DRIVER_SHOW:
        if (!dispatch_driver_target(state, action.value, 0, &action, &error)) {
            show_error_dialog(state, &error);
        }
        return;
    case APL_TUI_ACTION_OPEN_MEMBERSHIP_ADD_FORM: {
        if (active_team_id(state) == 0) {
            show_info_dialog(state, "No active team",
                             "Select an active team before adding a membership.");
            return;
        }
        copy_pending_membership_values(state, &action);
        if (action.driver[0] != '\0') {
            if (!dispatch_driver_target(state, action.driver, 1, &action, &error)) {
                show_error_dialog(state, &error);
            }
            return;
        }
        apl_driver *drivers = nullptr;
        size_t count = 0;
        if (apl_driver_service_list_for_team(state->app, active_team_id(state), false, &drivers,
                                             &count, &error) != APL_OK) {
            show_error_dialog(state, &error);
            return;
        }
        if (count == 0) {
            apl_driver_service_free_list(drivers);
            show_info_dialog(state, "No drivers",
                             "Create a driver profile first, then add a team membership.");
            return;
        }
        if (count == 1) {
            const int64_t driver_id = drivers[0].id;
            apl_driver_service_free_list(drivers);
            open_membership_add_dialog(state, driver_id, state->pending_start, state->pending_end,
                                       state->pending_role, state->pending_notes);
            return;
        }
        open_driver_choice(state, drivers, count, 1);
        apl_driver_service_free_list(drivers);
        return;
    }
    case APL_TUI_ACTION_CREATE_MEMBERSHIP_DIRECT:
        if (active_team_id(state) == 0) {
            show_info_dialog(state, "No active team",
                             "Select an active team before adding a membership.");
            return;
        }
        if (!dispatch_driver_target(state, action.driver, 3, &action, &error)) {
            show_error_dialog(state, &error);
        }
        return;
    case APL_TUI_ACTION_OPEN_MEMBERSHIP_END_FORM: {
        if (active_team_id(state) == 0) {
            show_info_dialog(state, "No active team",
                             "Select an active team before ending a membership.");
            return;
        }
        copy_pending_membership_values(state, &action);
        if (action.driver[0] != '\0') {
            if (!dispatch_driver_target(state, action.driver, 2, &action, &error)) {
                show_error_dialog(state, &error);
            }
            return;
        }
        apl_driver *drivers = nullptr;
        size_t count = 0;
        if (apl_driver_service_list_for_team(state->app, active_team_id(state), false, &drivers,
                                             &count, &error) != APL_OK) {
            show_error_dialog(state, &error);
            return;
        }
        if (count == 0) {
            apl_driver_service_free_list(drivers);
            show_info_dialog(state, "No drivers",
                             "There are no active-team drivers with a membership to end.");
            return;
        }
        if (count == 1) {
            const int64_t driver_id = drivers[0].id;
            apl_driver_service_free_list(drivers);
            if (!command_open_membership_end(state, driver_id, state->pending_end, &error)) {
                show_error_dialog(state, &error);
            }
            return;
        }
        open_driver_choice(state, drivers, count, 2);
        apl_driver_service_free_list(drivers);
        return;
    }
    case APL_TUI_ACTION_END_MEMBERSHIP_DIRECT:
        if (active_team_id(state) == 0) {
            show_info_dialog(state, "No active team",
                             "Select an active team before ending a membership.");
            return;
        }
        if (!dispatch_driver_target(state, action.driver, 4, &action, &error)) {
            show_error_dialog(state, &error);
        }
        return;
    case APL_TUI_ACTION_OPEN_SEASON_LIST:
        state->section = APL_TUI_SECTION_TEAMS;
        state->screen = SHELL_SCREEN_SEASON_LIST;
        if (load_seasons(state, 0, &error) != APL_OK) {
            show_error_dialog(state, &error);
        } else {
            sync_status_context(state);
            set_status(state, "Seasons/periods for the active team.");
        }
        return;
    case APL_TUI_ACTION_OPEN_SEASON_CREATE_FORM:
        if (active_team_id(state) == 0) {
            show_info_dialog(state, "No active team",
                             "Select an active team before creating a season.");
            return;
        }
        open_season_dialog(state, false, action.value, action.start, action.end, action.notes);
        return;
    case APL_TUI_ACTION_CREATE_SEASON_DIRECT:
        if (active_team_id(state) == 0) {
            show_info_dialog(state, "No active team",
                             "Select an active team before creating a season.");
            return;
        }
        if (!perform_season_create(state, action.value, action.start, action.end, action.notes,
                                   &error)) {
            show_error_dialog(state, &error);
        }
        return;
    case APL_TUI_ACTION_SEASON_SELECT_TARGET:
        if (active_team_id(state) == 0) {
            show_info_dialog(state, "No active team",
                             "Select an active team before selecting a season.");
            return;
        }
        if (!dispatch_season_select(state, action.value, &error)) {
            show_error_dialog(state, &error);
        }
        return;
    case APL_TUI_ACTION_OPEN_CAR_LIST:
        state->section = APL_TUI_SECTION_TEAMS;
        state->screen = SHELL_SCREEN_CAR_LIST;
        if (load_cars(state, 0, &error) != APL_OK) {
            show_error_dialog(state, &error);
        } else {
            sync_status_context(state);
            set_status(state, "Cars/chassis for the active team.");
        }
        return;
    case APL_TUI_ACTION_OPEN_CAR_CREATE_FORM:
        if (active_team_id(state) == 0) {
            show_info_dialog(state, "No active team",
                             "Select an active team before creating a car.");
            return;
        }
        open_car_dialog(state, false, action.value, action.notes, action.role);
        return;
    case APL_TUI_ACTION_CREATE_CAR_DIRECT:
        if (active_team_id(state) == 0) {
            show_info_dialog(state, "No active team",
                             "Select an active team before creating a car.");
            return;
        }
        if (!perform_car_create(state, action.value, action.notes, action.role, &error)) {
            show_error_dialog(state, &error);
        }
        return;
    case APL_TUI_ACTION_OPEN_CAR_SHOW:
        if (active_team_id(state) == 0) {
            show_info_dialog(state, "No active team",
                             "Select an active team before opening a car.");
            return;
        }
        if (!dispatch_car_show(state, action.value, &error)) {
            show_error_dialog(state, &error);
        }
        return;
    case APL_TUI_ACTION_OPEN_HELP: {
        char help[APL_TUI_COMMAND_HELP_MAX];
        const apl_tui_command_spec *spec = nullptr;
        if (action.value[0] != '\0') {
            spec = apl_tui_command_lookup(action.value);
        }
        apl_tui_command_help(spec, help, sizeof(help));
        show_info_dialog(state, "Command help", help);
        return;
    }
    case APL_TUI_ACTION_OPEN_ABOUT:
        show_info_dialog(state, "About apl1",
                         "apl1 is a local, user-maintained Formula 1 team management database. "
                         "It does not generate schedules, results, missing facts, or demo data.");
        return;
    case APL_TUI_ACTION_OPEN_VERSION: {
        char version[128];
        apl1_version_info(version, sizeof(version));
        show_info_dialog(state, "Version information", version);
        return;
    }
    case APL_TUI_ACTION_QUIT:
        state->quit = true;
        return;
    case APL_TUI_ACTION_NONE:
        break;
    }
}

static void handle_command_line_input(shell_state *const state, const ncinput *const input)
{
    const uint32_t id = input->id;
    if (id == NCKEY_ESC) {
        close_command_line(state);
        set_status(state, "Command cancelled.");
        return;
    }
    if (is_enter_key(id)) {
        execute_command_line(state);
        return;
    }
    if (id == NCKEY_UP) {
        apl_tui_command_line_history_previous(&state->command_line);
        update_command_suggestion(state);
        return;
    }
    if (id == NCKEY_DOWN) {
        apl_tui_command_line_history_next(&state->command_line);
        update_command_suggestion(state);
        return;
    }
    if (id == NCKEY_BACKSPACE || id == 127) {
        apl_tui_command_line_backspace(&state->command_line);
        update_command_suggestion(state);
        return;
    }
    if (id == NCKEY_TAB) {
        if (command_suggestion_applicable(state)) {
            apl_tui_command_line_set_text(&state->command_line, state->command_line.suggestion);
            update_command_suggestion(state);
        }
        return;
    }
    if (id >= 0x20 && id < PRETERUNICODEBASE && input->utf8[0] != '\0') {
        apl_tui_command_line_insert(&state->command_line, input->utf8);
        update_command_suggestion(state);
    }
}

/** Milliseconds on a monotonic-enough clock for click matching. */
static int64_t now_ms(void)
{
    struct timespec now = {};
    (void)timespec_get(&now, TIME_UTC);
    return (int64_t)now.tv_sec * 1000 + (int64_t)(now.tv_nsec / 1000000);
}

static bool point_in_rect(const apl_tui_rect rect, const int y, const int x)
{
    return y >= (int)rect.y && y < (int)(rect.y + rect.rows) && x >= (int)rect.x &&
           x < (int)(rect.x + rect.cols);
}

static void handle_dialog_mouse(shell_state *const state, const ncinput *const input)
{
    const apl_tui_layout *const layout = apl_tui_frame_layout(&state->frame);
    if (layout == nullptr || input->id != NCKEY_BUTTON1) {
        return;
    }
    const apl_tui_rect overlay = layout->overlay;
    if (!point_in_rect(overlay, input->y, input->x)) {
        return;
    }
    const unsigned local_row = (unsigned)(input->y - (int)overlay.y);
    const unsigned local_col = (unsigned)(input->x - (int)overlay.x);
    const unsigned control =
        apl_tui_dialog_control_at(&state->dialog, overlay.rows, overlay.cols, local_row, local_col);
    if (control == UINT_MAX) {
        return;
    }

    if (state->dialog.kind == APL_TUI_DIALOG_CHOICE) {
        apl_tui_dialog_select_option(&state->dialog, control);
    } else {
        if (state->dialog.kind == APL_TUI_DIALOG_FORM && control < state->dialog.field_count) {
            state->dialog.focus = control;
            return;
        }
        state->dialog.focus = control;
    }

    ncinput enter = {.id = NCKEY_ENTER};
    handle_dialog_input(state, &enter);
}

static void handle_list_mouse(shell_state *const state, const ncinput *const input)
{
    const apl_tui_layout *const layout = apl_tui_frame_layout(&state->frame);
    if (layout == nullptr) {
        return;
    }

    if (input->id == NCKEY_SCROLL_UP) {
        apl_tui_list_move(&state->list, -APL_TUI_SCROLL_STEP);
        return;
    }
    if (input->id == NCKEY_SCROLL_DOWN) {
        apl_tui_list_move(&state->list, APL_TUI_SCROLL_STEP);
        return;
    }
    if (input->id != NCKEY_BUTTON1 || !point_in_rect(layout->body, input->y, input->x)) {
        return;
    }

    size_t count = 0;
    int64_t record_id = 0;
    if (state->section == APL_TUI_SECTION_PEOPLE) {
        count = state->driver_count;
    } else if (state->section == APL_TUI_SECTION_HISTORY) {
        count = state->event_count;
    } else if (state->screen == SHELL_SCREEN_SEASON_LIST) {
        count = state->season_count;
    } else if (state->screen == SHELL_SCREEN_CAR_LIST) {
        count = state->car_count;
    } else {
        count = state->team_count;
    }

    const unsigned row = (unsigned)(input->y - (int)layout->body.y);
    unsigned row_offset = 0;
    if (state->section == APL_TUI_SECTION_HISTORY) {
        if (row == 1) {
            unsigned starts[5] = {};
            unsigned ends[5] = {};
            char filters[512];
            build_history_filter_bar(state, filters, sizeof(filters), starts, ends);
            const unsigned column = (unsigned)(input->x - (int)layout->body.x);
            static const uint32_t FILTER_KEYS[] = {'a', 'f', 'g', 'd', 'c'};
            for (unsigned button = 0; button < 5; ++button) {
                if (column >= starts[button] && column < ends[button] + 2) {
                    handle_history_input(state, FILTER_KEYS[button]);
                    return;
                }
            }
            return;
        }
        if (row < 3) {
            return;
        }
        row_offset = 3;
    }
    const size_t index = state->list.offset + row - row_offset;
    if (index >= count) {
        return;
    }
    apl_tui_list_select(&state->list, index);

    if (state->section == APL_TUI_SECTION_PEOPLE) {
        record_id = state->drivers[index].id;
    } else if (state->section == APL_TUI_SECTION_HISTORY) {
        record_id = state->events[index].id;
    } else if (state->screen == SHELL_SCREEN_SEASON_LIST) {
        record_id = state->seasons[index].id;
    } else if (state->screen == SHELL_SCREEN_CAR_LIST) {
        record_id = state->cars[index].id;
    } else {
        record_id = state->teams[index].id;
    }

    const int64_t current = now_ms();
    if (state->last_click_team_id == record_id &&
        current - state->last_click_ms <= APL_TUI_DOUBLE_CLICK_MS) {
        state->last_click_team_id = 0;
        state->last_click_ms = 0;
        if (state->section == APL_TUI_SECTION_PEOPLE) {
            open_selected_person(state);
        } else if (state->section == APL_TUI_SECTION_HISTORY) {
            handle_history_input(state, NCKEY_ENTER);
        } else if (state->screen == SHELL_SCREEN_SEASON_LIST) {
            open_selected_season(state);
        } else if (state->screen == SHELL_SCREEN_CAR_LIST) {
            open_selected_car(state);
        } else {
            open_selected_detail(state);
        }
    } else {
        state->last_click_team_id = record_id;
        state->last_click_ms = current;
    }
}

static void handle_mouse(shell_state *const state, const ncinput *const input)
{
    if (input->evtype == NCTYPE_RELEASE || input->evtype == NCTYPE_REPEAT) {
        return;
    }
    /* Some IDE terminals deliver the same press twice. Drop an identical event
     * inside a few milliseconds; it is far faster than a human double-click. */
    const int64_t mouse_now = now_ms();
    if (input->id == state->last_mouse_id && input->y == state->last_mouse_y &&
        input->x == state->last_mouse_x && mouse_now - state->last_mouse_ms <= 25) {
        return;
    }
    state->last_mouse_id = input->id;
    state->last_mouse_y = input->y;
    state->last_mouse_x = input->x;
    state->last_mouse_ms = mouse_now;

    if (apl_tui_dialog_active(&state->dialog)) {
        handle_dialog_mouse(state, input);
        return;
    }

    if (state->menu.open) {
        const apl_tui_layout *const layout = apl_tui_frame_layout(&state->frame);
        if (layout != nullptr) {
            const apl_tui_rect popup =
                apl_tui_menu_popup_rect(&state->menu, layout->rows, layout->cols);
            if (point_in_rect(popup, input->y, input->x)) {
                const unsigned row = (unsigned)(input->y - (int)popup.y);
                if (row >= 1 && row <= apl_tui_menu_item_count(state->menu.group)) {
                    state->menu.item_focus = row - 1;
                    const apl_tui_menu_action action = apl_tui_menu_activate(&state->menu);
                    handle_menu_action(state, action);
                }
                return;
            }
            if (point_in_rect(layout->window, input->y, input->x)) {
                const apl_tui_menu_group group = apl_tui_menu_group_at_column((unsigned)input->x);
                if (group < APL_TUI_MENU_GROUP_COUNT) {
                    apl_tui_menu_open(&state->menu, group);
                }
                return;
            }
        }
        apl_tui_menu_close(&state->menu);
        return;
    }

    const apl_tui_layout *const layout = apl_tui_frame_layout(&state->frame);
    if (input->id == NCKEY_BUTTON1 && layout != nullptr &&
        point_in_rect(layout->tabs, input->y, input->x)) {
        const unsigned row = (unsigned)(input->y - (int)layout->tabs.y);
        const unsigned content_rows = (layout->tabs.rows > layout->team_switcher.rows)
                                          ? layout->tabs.rows - layout->team_switcher.rows
                                          : layout->tabs.rows;
        if (row < content_rows) {
            const size_t index = state->tabs.offset + row;
            if (index < apl_tui_section_tab_count()) {
                apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_TABS);
                state->team_focus = false;
                apl_tui_list_select(&state->tabs, index);
                set_section(state, apl_tui_section_tab_at(index));
            }
        } else if (point_in_rect(layout->team_switcher, input->y, input->x)) {
            apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_TABS);
            state->team_focus = true;
            open_team_picker(state);
        }
        return;
    }
    if (input->id == NCKEY_BUTTON1 && layout != nullptr &&
        point_in_rect(layout->status, input->y, input->x)) {
        const unsigned columns = layout->status.cols;
        const unsigned column = (unsigned)(input->x - (int)layout->status.x);
        if (column < apl_tui_status_team_hit_width(&state->status, columns)) {
            apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_STATUS);
            open_team_picker(state);
        } else if (column >= apl_tui_status_command_hit_start(&state->status, columns)) {
            apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_STATUS);
            open_command_line(state);
        }
        return;
    }
    if (input->id == NCKEY_BUTTON1 && layout != nullptr &&
        point_in_rect(layout->window, input->y, input->x)) {
        const apl_tui_menu_group group = apl_tui_menu_group_at_column((unsigned)input->x);
        if (group < APL_TUI_MENU_GROUP_COUNT) {
            apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_WINDOW);
            apl_tui_menu_focus_group(&state->menu, group);
            apl_tui_menu_open(&state->menu, group);
        } else if (layout->window.cols > 48 &&
                   (unsigned)input->x >= keys_button_start(layout->window.cols)) {
            handle_menu_action(state, APL_TUI_MENU_ACTION_HELP_KEYBOARD);
        }
        return;
    }

    if (state->section == APL_TUI_SECTION_PEOPLE) {
        if (state->screen == SHELL_SCREEN_PEOPLE_LIST) {
            handle_list_mouse(state, input);
        }
        return;
    }
    if (state->section == APL_TUI_SECTION_HISTORY) {
        if (state->screen == SHELL_SCREEN_HISTORY_LIST) {
            handle_list_mouse(state, input);
        }
        return;
    }
    if (state->section != APL_TUI_SECTION_TEAMS) {
        return;
    }
    switch (state->screen) {
    case SHELL_SCREEN_TEAM_LIST:
    case SHELL_SCREEN_SEASON_LIST:
    case SHELL_SCREEN_CAR_LIST:
        handle_list_mouse(state, input);
        break;
    default:
        break;
    }
}

static void handle_dialog_input(shell_state *const state, const ncinput *const input)
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

static void open_selected_detail(shell_state *const state)
{
    const apl_team *const team = selected_team(state);
    if (team == nullptr) {
        return;
    }
    state->detail = *team;
    state->screen = SHELL_SCREEN_TEAM_DETAIL;
    set_status(state, "Press e to edit, s to set active, x to archive, Esc to go back.");
}

static void open_selected_season(shell_state *const state)
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

static void open_selected_car(shell_state *const state)
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

static void open_selected_person(shell_state *const state)
{
    if (state->driver_count == 0 || state->list.selected >= state->driver_count) {
        return;
    }
    open_person_detail(state, state->drivers[state->list.selected]);
}

static void handle_list_input(shell_state *const state, const uint32_t id)
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

static void handle_detail_input(shell_state *const state, const uint32_t id)
{
    switch (id) {
    case NCKEY_ESC:
        apl_tui_list_set_count(&state->list, state->team_count);
        state->screen = state->any_teams ? SHELL_SCREEN_TEAM_LIST : SHELL_SCREEN_WELCOME;
        return;
    case 'e':
    case 'E':
        if (state->detail.archived) {
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
        if (state->detail.archived) {
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

static void handle_welcome_input(shell_state *const state, const uint32_t id)
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

/** Shared arrow/page/home/end movement for any body list. */
static bool handle_body_list_navigation(shell_state *const state, const uint32_t id)
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

static void handle_season_list_input(shell_state *const state, const uint32_t id)
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

static void handle_season_detail_input(shell_state *const state, const uint32_t id)
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
        if (state->season_detail.archived) {
            set_status_warning(state, "Archived seasons are read-only in this view.");
        } else {
            open_season_dialog(state, true, state->season_detail.label,
                               state->season_detail.start_date, state->season_detail.end_date,
                               state->season_detail.notes);
        }
        return;
    case 'x':
    case 'X':
        if (state->season_detail.archived) {
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

static void handle_car_list_input(shell_state *const state, const uint32_t id)
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

static void handle_car_detail_input(shell_state *const state, const uint32_t id)
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
        if (state->car_detail.archived) {
            set_status_warning(state, "Archived cars are read-only in this view.");
        } else {
            open_car_dialog(state, true, state->car_detail.label, state->car_detail.chassis,
                            state->car_detail.notes);
        }
        return;
    case 'x':
    case 'X':
        if (state->car_detail.archived) {
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

static void handle_people_list_input(shell_state *const state, const uint32_t id)
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

static void handle_person_detail_input(shell_state *const state, const uint32_t id)
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
        if (state->driver_detail.archived) {
            set_status_warning(state, "Archived drivers are read-only in this view.");
        } else {
            open_driver_dialog(state, true, state->driver_detail.name,
                               state->driver_detail.short_name, state->driver_detail.notes);
        }
        return;
    case 'x':
    case 'X':
        if (state->driver_detail.archived) {
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

static void reload_history_with_status(shell_state *const state, const char *const message)
{
    apl_error error;
    apl_error_init(&error);
    if (load_history(state, &error) != APL_OK) {
        show_error_dialog(state, &error);
        return;
    }
    set_status(state, "%s", message);
}

static void handle_history_input(shell_state *const state, const uint32_t id)
{
    if (handle_body_list_navigation(state, id)) {
        return;
    }
    switch (id) {
    case NCKEY_ESC:
        set_section(state, APL_TUI_SECTION_TEAMS);
        return;
    case 'a':
    case 'A':
        state->history_all_teams = !state->history_all_teams;
        reload_history_with_status(state, state->history_all_teams
                                              ? "History scope: all teams (explicit)."
                                              : "History scope: active team.");
        return;
    case 'f':
    case 'F':
        state->history_entity_filter = (state->history_entity_filter + 1) % APL_TUI_HISTORY_FILTERS;
        reload_history_with_status(state, "Entity filter changed.");
        return;
    case 'g':
    case 'G':
        state->history_action_filter =
            (state->history_action_filter + 1) % history_action_filter_count();
        reload_history_with_status(state, "Action filter changed.");
        return;
    case 'd':
    case 'D':
        state->history_date_preset = (state->history_date_preset + 1) % 4;
        reload_history_with_status(state, "Date filter changed.");
        return;
    case 'c':
    case 'C':
        state->history_all_teams = false;
        state->history_entity_filter = 0;
        state->history_action_filter = 0;
        state->history_date_preset = 0;
        reload_history_with_status(state, "History filters cleared; scope is the active team.");
        return;
    case 'q':
    case 'Q':
        state->quit = true;
        return;
    default:
        break;
    }
    if (is_enter_key(id) && state->event_count > 0 && state->list.selected < state->event_count) {
        const int64_t event_id = state->events[state->list.selected].id;
        apl_activity_event event = {};
        apl_activity_change *changes = nullptr;
        size_t change_count = 0;
        apl_error error;
        apl_error_init(&error);
        if (apl_activity_service_get(state->app, event_id, &event, &changes, &change_count,
                                     &error) != APL_OK) {
            show_error_dialog(state, &error);
            return;
        }
        char message[APL_TUI_DIALOG_MESSAGE_MAX];
        int used = snprintf(
            message, sizeof(message), "%s\nAction: %s\nRecord: %s #%lld\nWhen: %s\n", event.summary,
            event.action, event.entity_type, (long long)event.entity_id, event.occurred_at);
        for (size_t index = 0; index < change_count && used > 0 && (size_t)used < sizeof(message);
             ++index) {
            used += snprintf(message + (size_t)used, sizeof(message) - (size_t)used,
                             "\n%s: %s -> %s", changes[index].field,
                             changes[index].old_missing ? "(absent)" : changes[index].old_value,
                             changes[index].new_missing ? "(absent)" : changes[index].new_value);
        }
        apl_activity_service_free_changes(changes);
        apl_tui_dialog_open_error(&state->dialog, "Activity detail", message);
        state->dialog_context = SHELL_DIALOG_CONTEXT_ACTIVITY_DETAIL;
        apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
        return;
    }
}

static void handle_window_input(shell_state *const state, const ncinput *const input)
{
    const uint32_t id = input->id;
    if (state->menu.open) {
        switch (id) {
        case NCKEY_LEFT:
            apl_tui_menu_move_group(&state->menu, -1);
            return;
        case NCKEY_RIGHT:
            apl_tui_menu_move_group(&state->menu, 1);
            return;
        case NCKEY_UP:
            apl_tui_menu_move_item(&state->menu, -1);
            return;
        case NCKEY_DOWN:
            apl_tui_menu_move_item(&state->menu, 1);
            return;
        case NCKEY_ESC:
            apl_tui_menu_close(&state->menu);
            return;
        case NCKEY_TAB:
            apl_tui_menu_close(&state->menu);
            apl_tui_focus_cycle(&state->focus, ncinput_shift_p(input) ? -1 : 1);
            return;
        default:
            break;
        }
        if (is_enter_key(id)) {
            const apl_tui_menu_action action = apl_tui_menu_activate(&state->menu);
            handle_menu_action(state, action);
        }
        return;
    }

    switch (id) {
    case NCKEY_LEFT:
        apl_tui_menu_move_group(&state->menu, -1);
        return;
    case NCKEY_RIGHT:
        apl_tui_menu_move_group(&state->menu, 1);
        return;
    case NCKEY_DOWN:
    case NCKEY_UP:
        apl_tui_menu_open(&state->menu, state->menu.group_focus);
        return;
    case NCKEY_ESC:
        apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_BODY);
        return;
    case NCKEY_TAB:
        apl_tui_focus_cycle(&state->focus, ncinput_shift_p(input) ? -1 : 1);
        return;
    default:
        break;
    }
    if (is_enter_key(id)) {
        apl_tui_menu_open(&state->menu, state->menu.group_focus);
    }
}

static void handle_tabs_input(shell_state *const state, const ncinput *const input)
{
    switch (input->id) {
    case NCKEY_UP:
        if (state->team_focus) {
            state->team_focus = false;
            return;
        }
        apl_tui_list_move(&state->tabs, -1);
        set_section(state, apl_tui_section_tab_at(state->tabs.selected));
        return;
    case NCKEY_DOWN:
        if (!state->team_focus && state->tabs.selected + 1 >= apl_tui_section_tab_count()) {
            state->team_focus = true;
            return;
        }
        if (state->team_focus) {
            return;
        }
        apl_tui_list_move(&state->tabs, 1);
        set_section(state, apl_tui_section_tab_at(state->tabs.selected));
        return;
    case NCKEY_PGUP:
        state->team_focus = false;
        apl_tui_list_page(&state->tabs, -1);
        set_section(state, apl_tui_section_tab_at(state->tabs.selected));
        return;
    case NCKEY_PGDOWN:
        state->team_focus = false;
        apl_tui_list_page(&state->tabs, 1);
        set_section(state, apl_tui_section_tab_at(state->tabs.selected));
        return;
    case NCKEY_HOME:
        state->team_focus = false;
        apl_tui_list_home(&state->tabs);
        set_section(state, apl_tui_section_tab_at(state->tabs.selected));
        return;
    case NCKEY_END:
        state->team_focus = false;
        apl_tui_list_end(&state->tabs);
        set_section(state, apl_tui_section_tab_at(state->tabs.selected));
        return;
    case NCKEY_TAB:
        apl_tui_focus_cycle(&state->focus, ncinput_shift_p(input) ? -1 : 1);
        return;
    case NCKEY_ESC:
        if (state->team_focus) {
            state->team_focus = false;
        } else {
            apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_BODY);
        }
        return;
    default:
        break;
    }
    if (is_enter_key(input->id)) {
        if (state->team_focus) {
            open_team_picker(state);
        } else {
            set_section(state, apl_tui_section_tab_at(state->tabs.selected));
        }
    }
}

static void handle_status_input(shell_state *const state, const ncinput *const input)
{
    if (input->id == NCKEY_TAB) {
        apl_tui_focus_cycle(&state->focus, ncinput_shift_p(input) ? -1 : 1);
    } else if (input->id == NCKEY_ESC) {
        apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_BODY);
    } else if (is_enter_key(input->id) || input->id == 's' || input->id == 'S') {
        open_team_picker(state);
    }
}

static void handle_input(shell_state *const state, const ncinput *const input)
{
    const uint32_t id = input->id;
    /* Kitty/JetBrains-style terminals emit release events for each press. Only
     * presses (and repeats, for key auto-repeat) are actionable. */
    if (input->evtype == NCTYPE_RELEASE) {
        return;
    }
    if (apl_tui_dialog_active(&state->dialog)) {
        handle_dialog_input(state, input);
        return;
    }
    if (apl_tui_command_line_is_open(&state->command_line)) {
        handle_command_line_input(state, input);
        return;
    }
    if (id == ':') {
        open_command_line(state);
        return;
    }
    if (id == NCKEY_F10) {
        apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_WINDOW);
        return;
    }
    if (id == NCKEY_F02) {
        open_team_picker(state);
        return;
    }
    if (id == 't' || id == 'T') {
        open_team_picker(state);
        return;
    }
    if (id == 'q' || id == 'Q') {
        state->quit = true;
        return;
    }
    if (id == 'm' || id == 'M') {
        if (state->section == APL_TUI_SECTION_PEOPLE) {
            if (state->screen == SHELL_SCREEN_PERSON_DETAIL) {
                handle_person_detail_input(state, id);
            } else {
                handle_people_list_input(state, id);
            }
            return;
        }
        apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_WINDOW);
        apl_tui_menu_open(&state->menu, state->menu.group_focus);
        return;
    }
    /* Easy view-switch keys for layouts where [ and ] need a modifier.
     * The bracket keys remain as aliases. */
    if (!state->menu.open && (id == ',' || id == '[')) {
        switch_section(state, false);
        return;
    }
    if (!state->menu.open && (id == '.' || id == ']')) {
        switch_section(state, true);
        return;
    }
    if (state->focus.region == APL_TUI_FOCUS_WINDOW) {
        handle_window_input(state, input);
        return;
    }
    if (state->focus.region == APL_TUI_FOCUS_TABS) {
        handle_tabs_input(state, input);
        return;
    }
    if (state->focus.region == APL_TUI_FOCUS_STATUS) {
        handle_status_input(state, input);
        return;
    }
    if (id == NCKEY_TAB) {
        apl_tui_menu_close(&state->menu);
        apl_tui_focus_cycle(&state->focus, ncinput_shift_p(input) ? -1 : 1);
        return;
    }
    if (state->section == APL_TUI_SECTION_PEOPLE) {
        if (state->screen == SHELL_SCREEN_PERSON_DETAIL) {
            handle_person_detail_input(state, id);
        } else {
            handle_people_list_input(state, id);
        }
        return;
    }
    if (state->section == APL_TUI_SECTION_HISTORY) {
        handle_history_input(state, id);
        return;
    }
    if (state->section != APL_TUI_SECTION_TEAMS) {
        if (id == 'q' || id == 'Q') {
            state->quit = true;
        } else if (id == '[') {
            switch_section(state, false);
        } else if (id == ']') {
            switch_section(state, true);
        }
        return;
    }

    switch (state->screen) {
    case SHELL_SCREEN_WELCOME:
        handle_welcome_input(state, id);
        break;
    case SHELL_SCREEN_TEAM_LIST:
        handle_list_input(state, id);
        break;
    case SHELL_SCREEN_TEAM_DETAIL:
        handle_detail_input(state, id);
        break;
    case SHELL_SCREEN_SEASON_LIST:
        handle_season_list_input(state, id);
        break;
    case SHELL_SCREEN_SEASON_DETAIL:
        handle_season_detail_input(state, id);
        break;
    case SHELL_SCREEN_CAR_LIST:
        handle_car_list_input(state, id);
        break;
    case SHELL_SCREEN_CAR_DETAIL:
        handle_car_detail_input(state, id);
        break;
    case SHELL_SCREEN_PLACEHOLDER:
    case SHELL_SCREEN_PEOPLE_LIST:
    case SHELL_SCREEN_PERSON_DETAIL:
    case SHELL_SCREEN_HISTORY_LIST:
        if (id == 'q' || id == 'Q') {
            state->quit = true;
        }
        break;
    }
}

/** Load the initial team state and choose the starting screen. */
static apl_status initialize_state(shell_state *const state, apl_error *const error)
{
    state->section = apl_tui_section_default();
    state->screen = SHELL_SCREEN_WELCOME;
    state->include_archived = false;
    state->include_archived_seasons = false;
    state->include_archived_cars = false;
    state->include_archived_drivers = false;
    state->people_all_teams = false;
    state->history_all_teams = false;
    state->selected_season_id = 0;
    apl_tui_list_init(&state->list, 10);
    apl_tui_list_init(&state->tabs, 6);
    apl_tui_list_set_count(&state->tabs, apl_tui_section_tab_count());
    apl_tui_dialog_init(&state->dialog);
    apl_tui_menu_init(&state->menu);
    apl_tui_focus_init(&state->focus);
    apl_tui_status_init(&state->status);
    apl_tui_command_line_init(&state->command_line);
    sync_status_context(state);

    apl_status status = detect_any_teams(state, error);
    if (status != APL_OK) {
        return status;
    }
    status = refresh_active(state, error);
    if (status != APL_OK) {
        return status;
    }

    if (!state->any_teams) {
        state->screen = SHELL_SCREEN_WELCOME;
        return APL_OK;
    }

    status = refresh_teams(state, state->active_team.id, error);
    if (status != APL_OK) {
        return status;
    }

    state->screen = SHELL_SCREEN_TEAM_LIST;
    if (state->active_status == APL_ACTIVE_TEAM_FALLBACK) {
        set_status(state,
                   "Previously selected team was archived or missing; active team is now '%s'.",
                   state->active_team.name);
    } else if (state->active_status == APL_ACTIVE_TEAM_ACTIVE) {
        set_status_success(state, "Active team: %s", state->active_team.name);
    } else if (state->active_status == APL_ACTIVE_TEAM_NONE && state->team_count > 0) {
        set_status(state, "No active team selected. Use s to choose one.");
    }

    const apl_tui_layout *const layout = apl_tui_frame_layout(&state->frame);
    apl_tui_list_set_viewport_rows(&state->list, layout->body.rows);
    return APL_OK;
}

apl_status apl_tui_shell_run(const apl_tui *const tui, apl_app *const app, apl_error *const error)
{
    if (tui == nullptr || tui->notcurses == nullptr || app == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "TUI shell requires a started TUI and app");
        return APL_ERR_INVALID_ARGUMENT;
    }

    shell_state state = {.app = app};
    const bool mouse_enabled = notcurses_mice_enable(tui->notcurses, NCMICE_BUTTON_EVENT) == 0;
    const char *const no_color = getenv("NO_COLOR");
    const bool color_capable = notcurses_canchangecolor(tui->notcurses) > 0 &&
                               (no_color == nullptr || no_color[0] == '\0');
    apl_tui_theme_init(&state.theme, color_capable);
    apl_status status = apl_tui_frame_start(&state.frame, tui, error);
    if (status != APL_OK) {
        apl_tui_frame_stop(&state.frame);
        return status;
    }

    status = initialize_state(&state, error);
    if (status != APL_OK) {
        apl_tui_frame_stop(&state.frame);
        apl_team_service_free_list(state.teams);
        return status;
    }

    while (!state.quit) {
        apl_tui_frame_set_overlay(&state.frame, apl_tui_dialog_active(&state.dialog));
        apl_tui_frame_set_popup(&state.frame, false, (apl_tui_rect){});
        status = apl_tui_frame_sync(&state.frame, tui, error);
        if (status != APL_OK) {
            break;
        }

        if (apl_tui_frame_normal(&state.frame)) {
            const apl_tui_layout *const layout = apl_tui_frame_layout(&state.frame);
            if (state.menu.open) {
                apl_tui_frame_set_popup(
                    &state.frame, true,
                    apl_tui_menu_popup_rect(&state.menu, layout->rows, layout->cols));
                status = apl_tui_frame_sync(&state.frame, tui, error);
                if (status != APL_OK) {
                    break;
                }
            }
            unsigned list_rows =
                (layout->body.rows > 1) ? layout->body.rows - 1 : layout->body.rows;
            if (state.section == APL_TUI_SECTION_HISTORY) {
                list_rows = (layout->body.rows > 4) ? layout->body.rows - 4 : 0;
            }
            apl_tui_list_set_viewport_rows(&state.list, list_rows);
            apl_tui_frame_begin(&state.frame);
            draw_screen(&state);
        }
        status = apl_tui_frame_render(&state.frame, error);
        if (status != APL_OK) {
            break;
        }

        ncinput input = {};
        const uint32_t id = notcurses_get_blocking(tui->notcurses, &input);
        if (id == (uint32_t)-1) {
            apl_error_set(error, APL_ERR_TUI_INPUT, "terminal input became unavailable");
            status = APL_ERR_TUI_INPUT;
            break;
        }
        if (id == NCKEY_EOF) {
            apl_error_set(error, APL_ERR_TUI_INPUT, "terminal input reached EOF");
            status = APL_ERR_TUI_INPUT;
            break;
        }
        if (id == NCKEY_RESIZE) {
            continue;
        }
        if (!apl_tui_frame_normal(&state.frame)) {
            if (id == 'q' || id == 'Q') {
                state.quit = true;
            }
            continue;
        }
        if (nckey_mouse_p(id)) {
            handle_mouse(&state, &input);
            continue;
        }

        handle_input(&state, &input);
    }

    if (mouse_enabled) {
        (void)notcurses_mice_disable(tui->notcurses);
    }
    apl_tui_frame_stop(&state.frame);
    apl_team_service_free_list(state.teams);
    free_reference_lists(&state);
    free_picker_teams(&state);
    if (status == APL_OK) {
        apl_error_clear(error);
    }
    return status;
}
