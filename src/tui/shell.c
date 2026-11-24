#include "tui/shell.h"

#include <limits.h>
#include <notcurses/notcurses.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    SHELL_SCREEN_PLACEHOLDER,
} shell_screen;

typedef enum shell_pending_action {
    SHELL_PENDING_NONE = 0,
    SHELL_PENDING_ARCHIVE_TEAM,
} shell_pending_action;

typedef enum shell_dialog_context {
    SHELL_DIALOG_CONTEXT_NONE = 0,
    SHELL_DIALOG_CONTEXT_TEAM_PICKER,
    SHELL_DIALOG_CONTEXT_TEAM_CREATE,
    SHELL_DIALOG_CONTEXT_TEAM_EDIT,
} shell_dialog_context;

constexpr int APL_TUI_SCROLL_STEP = 3;
constexpr int64_t APL_TUI_DOUBLE_CLICK_MS = 500;

typedef struct shell_state {
    apl_app *app;
    apl_tui_frame frame;
    apl_tui_section section;
    shell_screen screen;
    apl_tui_list list;
    apl_tui_list tabs;
    apl_team *teams;
    size_t team_count;
    bool include_archived;
    bool any_teams;
    apl_team active_team;
    apl_active_team_status active_status;
    apl_team detail;
    apl_tui_dialog dialog;
    apl_tui_menu menu;
    apl_tui_focus focus;
    apl_tui_status status;
    apl_tui_theme theme;
    apl_tui_command_line command_line;
    shell_dialog_context dialog_context;
    int64_t dialog_team_id;
    apl_team *picker_teams;
    size_t picker_team_count;
    bool picker_include_archived;
    bool team_focus;
    shell_pending_action pending;
    int64_t pending_team_id;
    int64_t last_click_team_id;
    int64_t last_click_ms;
    bool quit;
} shell_state;

static void handle_dialog_input(shell_state *state, const ncinput *input);
static void open_selected_detail(shell_state *state);

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

static void sync_status_context(shell_state *const state)
{
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

static bool perform_create(shell_state *const state, const char *const name, const char *const notes,
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

    apl_error_init(error);
    apl_status reload = refresh_active(state, error);
    if (reload == APL_OK) {
        reload = refresh_teams(state, 0, error);
    }
    if (reload != APL_OK) {
        show_error_dialog(state, error);
        return;
    }
    state->screen = state->any_teams ? SHELL_SCREEN_TEAM_LIST : SHELL_SCREEN_WELCOME;
    if (was_active && active_status_has_team(state->active_status)) {
        set_status_success(state, "Team archived. Active team switched to '%s'.", state->active_team.name);
    } else if (was_active) {
        set_status_success(state, "Team archived. No active team remains.");
    } else {
        set_status_success(state, "Team archived. Its history remains available in the archived view.");
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
    if (refresh_active(state, error) != APL_OK) {
        show_error_dialog(state, error);
        return;
    }
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
    return apl_team_service_list(state->app, state->picker_include_archived,
                                 &state->picker_teams, &state->picker_team_count, error);
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
        const bool already_active = active_status_has_team(state->active_status) &&
                                    state->active_team.id == team.id;
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

static void set_section(shell_state *const state, const apl_tui_section section)
{
    state->section = section;
    const size_t tab = apl_tui_section_tab_index(section);
    if (tab != SIZE_MAX) {
        apl_tui_list_select(&state->tabs, tab);
    }
    if (section == APL_TUI_SECTION_TEAMS) {
        state->screen = state->any_teams ? SHELL_SCREEN_TEAM_LIST : SHELL_SCREEN_WELCOME;
    } else {
        state->screen = SHELL_SCREEN_PLACEHOLDER;
    }
    sync_status_context(state);
}

static void switch_section(shell_state *const state, const bool forward)
{
    set_section(state, forward ? apl_tui_section_next(state->section)
                               : apl_tui_section_previous(state->section));
}

static void handle_menu_action(shell_state *const state,
                               const apl_tui_menu_action action)
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
    case APL_TUI_MENU_ACTION_DATABASE_INFO:
        set_section(state, APL_TUI_SECTION_SETTINGS);
        show_info_dialog(
            state, "Database information",
            "The SQLite database is chosen at launch with --database or the default data "
            "directory. Schema upgrades are applied by the migration runner; no settings are "
            "editable in this phase.");
        set_status(state, "Database information.");
        return;
    case APL_TUI_MENU_ACTION_HELP_KEYBOARD:
        show_info_dialog(state, "Keyboard and mouse help",
                         "F10 focuses the window bar. Tab cycles focus. Arrow keys move and "
                         "Enter activates. Esc closes menus and dialogs. Mouse clicks use the "
                         "same actions as the keyboard. q quits.");
        return;
    case APL_TUI_MENU_ACTION_HELP_COMMANDS:
        show_info_dialog(state, "Command help",
                         "Command entry is opened with ':'. Use it for navigation and team "
                         "actions; commands never execute shell or SQL text.");
        return;
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
        const bool focused = state->focus.region == APL_TUI_FOCUS_WINDOW &&
                             (state->menu.open ? state->menu.group == group
                                               : state->menu.group_focus == group);
        apl_tui_theme_apply(&state->theme, plane,
                             focused ? APL_TUI_ROLE_WINDOW_FOCUS : APL_TUI_ROLE_WINDOW_BAR);
        apl_tui_frame_draw_text(plane, 0, (int)apl_tui_menu_heading_offset(group),
                                apl_tui_menu_group_label(group));
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
    const unsigned content_rows =
        (layout->tabs.rows > layout->team_switcher.rows)
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
        const char *const label = compact ? apl_tui_section_short_label(section)
                                          : apl_tui_section_label(section);
        char line[64];
        (void)snprintf(line, sizeof(line), "%c%c %s", active ? '*' : ' ', focused ? '>' : ' ',
                       label);
        apl_tui_theme_apply(&state->theme, rail,
                             focused ? APL_TUI_ROLE_TAB_FOCUS
                                     : (active ? APL_TUI_ROLE_TAB_ACTIVE
                                               : APL_TUI_ROLE_TAB_INACTIVE));
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

    apl_tui_theme_apply(&state->theme, rail,
                         focused ? APL_TUI_ROLE_TEAM_FOCUS : APL_TUI_ROLE_TEAM);
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
    apl_tui_frame_draw_line(body, row++, "  People: not implemented in this phase");
    apl_tui_frame_draw_line(body, row++, "  Cars: not implemented in this phase");
    apl_tui_frame_draw_line(body, row++, "  Money: not implemented in this phase");
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
    apl_tui_frame_draw_line(body, 3, "Use the tab rail or [ and ] to switch views.");
    apl_tui_frame_draw_line(body, 5, "No records are generated for this view.");
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

    apl_tui_theme_apply(&state->theme, overlay, APL_TUI_ROLE_DIALOG_TITLE);
    apl_tui_frame_draw_text(overlay, 1, 2, state->dialog.title);
    apl_tui_theme_apply(&state->theme, overlay, APL_TUI_ROLE_DIALOG);
    apl_tui_frame_draw_text(overlay, 2, 2, state->dialog.message);

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
            char line[APL_TUI_DIALOG_FIELD_LABEL_MAX + APL_TUI_DIALOG_FIELD_VALUE_MAX + 16];
            (void)snprintf(line, sizeof(line), "%c %s: %s%s", focused ? '>' : ' ', field->label,
                           field->value, field->invalid ? "  !" : "");
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
        const unsigned message_rows = apl_tui_frame_draw_wrapped(
            overlay, 3, 2, cols > 4 ? cols - 4 : cols, state->dialog.message);
        (void)message_rows;
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
        const unsigned message_rows = apl_tui_frame_draw_wrapped(
            overlay, 3, 2, cols > 4 ? cols - 4 : cols, state->dialog.message);
        (void)message_rows;
        apl_tui_theme_apply(&state->theme, overlay, APL_TUI_ROLE_SELECTION);
        apl_tui_frame_draw_text(overlay, button_row, 2, "[ OK ]");
        break;
    }
    case APL_TUI_DIALOG_NONE:
        break;
    }
    apl_tui_theme_reset(overlay);
}

static void draw_screen(shell_state *const state)
{
    draw_window_bar(state);
    draw_tab_rail(state);
    draw_team_switcher(state);

    if (state->section != APL_TUI_SECTION_TEAMS) {
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
        case SHELL_SCREEN_PLACEHOLDER:
            draw_placeholder(state);
            break;
        }
    }

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
    apl_tui_command_line_set_suggestion(&state->command_line, suggestion);
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
    const unsigned control = apl_tui_dialog_control_at(
        &state->dialog, overlay.rows, overlay.cols, local_row, local_col);
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

    const unsigned row = (unsigned)(input->y - (int)layout->body.y);
    const size_t index = state->list.offset + row;
    if (index >= state->team_count) {
        return;
    }
    apl_tui_list_select(&state->list, index);

    const apl_team *const team = &state->teams[index];
    const int64_t current = now_ms();
    if (state->last_click_team_id == team->id &&
        current - state->last_click_ms <= APL_TUI_DOUBLE_CLICK_MS) {
        state->last_click_team_id = 0;
        state->last_click_ms = 0;
        open_selected_detail(state);
    } else {
        state->last_click_team_id = team->id;
        state->last_click_ms = current;
    }
}

static void handle_mouse(shell_state *const state, const ncinput *const input)
{
    if (input->evtype == NCTYPE_RELEASE || input->evtype == NCTYPE_REPEAT) {
        return;
    }
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
                const apl_tui_menu_group group =
                    apl_tui_menu_group_at_column((unsigned)input->x);
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
        const unsigned content_rows =
            (layout->tabs.rows > layout->team_switcher.rows)
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
        }
        return;
    }

    if (state->section != APL_TUI_SECTION_TEAMS) {
        return;
    }
    switch (state->screen) {
    case SHELL_SCREEN_TEAM_LIST:
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
    state->pending = SHELL_PENDING_NONE;
    state->pending_team_id = 0;
    close_shell_dialog(state);

    if (pending == SHELL_PENDING_ARCHIVE_TEAM) {
        if (result == APL_TUI_DIALOG_ACCEPTED) {
            perform_archive(state, pending_team, &error);
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
        if (!state->team_focus &&
            state->tabs.selected + 1 >= apl_tui_section_tab_count()) {
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
        apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_WINDOW);
        apl_tui_menu_open(&state->menu, state->menu.group_focus);
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
    case SHELL_SCREEN_PLACEHOLDER:
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
    apl_tui_theme_init(&state.theme, notcurses_canchangecolor(tui->notcurses) > 0);
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
            apl_tui_list_set_viewport_rows(&state.list, layout->body.rows);
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
    free_picker_teams(&state);
    if (status == APL_OK) {
        apl_error_clear(error);
    }
    return status;
}
