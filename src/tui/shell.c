#include "tui/shell.h"

#include <notcurses/notcurses.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app/team_service.h"
#include "app/version.h"
#include "tui/dialog.h"
#include "tui/focus.h"
#include "tui/frame.h"
#include "tui/list.h"
#include "tui/menu.h"
#include "tui/navigation.h"

typedef enum shell_screen {
    SHELL_SCREEN_WELCOME = 0,
    SHELL_SCREEN_TEAM_LIST,
    SHELL_SCREEN_TEAM_DETAIL,
    SHELL_SCREEN_TEAM_FORM,
    SHELL_SCREEN_PLACEHOLDER,
} shell_screen;

typedef enum shell_pending_action {
    SHELL_PENDING_NONE = 0,
    SHELL_PENDING_DISCARD_FORM,
    SHELL_PENDING_ARCHIVE_TEAM,
} shell_pending_action;

typedef struct shell_form {
    bool editing;
    int64_t team_id;
    char name[APL_TEAM_NAME_MAX];
    char notes[APL_TEAM_NOTES_MAX];
    char original_name[APL_TEAM_NAME_MAX];
    char original_notes[APL_TEAM_NOTES_MAX];
    unsigned focus; /* 0 = name, 1 = notes */
} shell_form;

constexpr size_t SHELL_STATUS_MAX = 320;
constexpr int APL_TUI_SCROLL_STEP = 3;
constexpr int64_t APL_TUI_DOUBLE_CLICK_MS = 500;

typedef struct shell_state {
    apl_app *app;
    apl_tui_frame frame;
    apl_tui_section section;
    shell_screen screen;
    shell_screen form_return_screen;
    apl_tui_list list;
    apl_team *teams;
    size_t team_count;
    bool include_archived;
    bool any_teams;
    apl_team active_team;
    apl_active_team_status active_status;
    apl_team detail;
    shell_form form;
    apl_tui_dialog dialog;
    apl_tui_menu menu;
    apl_tui_focus focus;
    shell_pending_action pending;
    int64_t pending_team_id;
    char status[SHELL_STATUS_MAX];
    int64_t last_click_team_id;
    int64_t last_click_ms;
    bool quit;
} shell_state;

static void handle_dialog_input(shell_state *state, const ncinput *input);
static void open_selected_detail(shell_state *state);

static void set_status(shell_state *const state, const char *const format, ...)
{
    va_list args;
    va_start(args);
    (void)vsnprintf(state->status, sizeof(state->status), format, args);
    va_end(args);
}

static void set_status_from_error(shell_state *const state, const char *const context,
                                  const apl_error *const error)
{
    const char *const message = apl_error_message(error);
    if (message != nullptr && message[0] != '\0') {
        set_status(state, "%s: %s", context, message);
    } else {
        set_status(state, "%s", context);
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

static void focus_error_field(shell_state *const state, const apl_error *const error)
{
    if (apl_error_code(error) != APL_ERR_INVALID_ARGUMENT &&
        apl_error_code(error) != APL_ERR_CONFLICT) {
        return;
    }
    state->form.focus = (strstr(apl_error_message(error), "notes") != nullptr) ? 1 : 0;
}

static apl_status refresh_active(shell_state *const state, apl_error *const error)
{
    return apl_team_service_active(state->app, &state->active_team, &state->active_status, error);
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

static bool form_is_dirty(const shell_state *const state)
{
    return strcmp(state->form.name, state->form.original_name) != 0 ||
           strcmp(state->form.notes, state->form.original_notes) != 0;
}

static void open_create_form(shell_state *const state)
{
    state->form = (shell_form){};
    state->form_return_screen = state->screen;
    state->screen = SHELL_SCREEN_TEAM_FORM;
    set_status(state, "Name is required and must be unique. Notes are optional. Tab moves fields.");
}

static void open_edit_form(shell_state *const state)
{
    state->form = (shell_form){
        .editing = true,
        .team_id = state->detail.id,
        .focus = 0,
    };
    (void)snprintf(state->form.name, sizeof(state->form.name), "%s", state->detail.name);
    (void)snprintf(state->form.notes, sizeof(state->form.notes), "%s", state->detail.notes);
    (void)snprintf(state->form.original_name, sizeof(state->form.original_name), "%s",
                   state->detail.name);
    (void)snprintf(state->form.original_notes, sizeof(state->form.original_notes), "%s",
                   state->detail.notes);
    state->form_return_screen = SHELL_SCREEN_TEAM_DETAIL;
    state->screen = SHELL_SCREEN_TEAM_FORM;
    set_status(state, "Editing team. Enter saves; Esc cancels.");
}

static void close_form(shell_state *const state)
{
    if (state->form.editing) {
        state->screen = SHELL_SCREEN_TEAM_DETAIL;
        return;
    }
    state->screen = (state->form_return_screen == SHELL_SCREEN_TEAM_FORM)
                        ? SHELL_SCREEN_TEAM_LIST
                        : state->form_return_screen;
    if (state->screen == SHELL_SCREEN_WELCOME && state->any_teams) {
        state->screen = SHELL_SCREEN_TEAM_LIST;
    }
}

static void show_error_dialog(shell_state *const state, const apl_error *const error)
{
    set_status_from_error(state, "Operation failed", error);
    apl_tui_dialog_open_error(&state->dialog, "Operation failed", apl_error_message(error));
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    state->pending = SHELL_PENDING_NONE;
}

static void show_info_dialog(shell_state *const state, const char *const title,
                             const char *const message)
{
    apl_tui_dialog_open_error(&state->dialog, title, message);
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    state->pending = SHELL_PENDING_NONE;
}

static void perform_create(shell_state *const state, apl_error *const error)
{
    apl_team created = {};
    const apl_status status =
        apl_team_service_create(state->app, state->form.name, state->form.notes, &created, error);
    if (status != APL_OK) {
        focus_error_field(state, error);
        set_status_from_error(state, "Cannot create team", error);
        return;
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
        return;
    }
    state->screen = SHELL_SCREEN_TEAM_DETAIL;
    set_status(state,
               "Team created. Next, consider adding people, seasons, money, or events in later "
               "phases. Press Esc for the team list.");
}

static void perform_update(shell_state *const state, apl_error *const error)
{
    apl_team updated = {};
    const apl_status status = apl_team_service_update(
        state->app, state->form.team_id, state->form.name, state->form.notes, &updated, error);
    if (status != APL_OK) {
        focus_error_field(state, error);
        set_status_from_error(state, "Cannot save team", error);
        return;
    }

    state->detail = updated;
    apl_error_init(error);
    apl_status reload = refresh_active(state, error);
    if (reload == APL_OK) {
        reload = refresh_teams(state, updated.id, error);
    }
    if (reload != APL_OK) {
        show_error_dialog(state, error);
        return;
    }
    state->screen = SHELL_SCREEN_TEAM_DETAIL;
    set_status(state, "Changes saved.");
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
        set_status(state, "Team archived. Active team switched to '%s'.", state->active_team.name);
    } else if (was_active) {
        set_status(state, "Team archived. No active team remains.");
    } else {
        set_status(state, "Team archived. Its history remains available in the archived view.");
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
    set_status(state, "Active team: %s", state->active_team.name);
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

static void set_section(shell_state *const state, const apl_tui_section section)
{
    state->section = section;
    if (section == APL_TUI_SECTION_TEAMS) {
        state->screen = state->any_teams ? SHELL_SCREEN_TEAM_LIST : SHELL_SCREEN_WELCOME;
    } else {
        state->screen = SHELL_SCREEN_PLACEHOLDER;
    }
    state->status[0] = '\0';
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

static void append_utf8(char *const buffer, const size_t capacity, const char *const text)
{
    if (buffer == nullptr || text == nullptr || capacity == 0) {
        return;
    }
    const size_t length = strlen(buffer);
    const size_t addition = strlen(text);
    if (length + addition >= capacity) {
        return;
    }
    memcpy(buffer + length, text, addition + 1);
}

static void backspace_utf8(char *const buffer)
{
    size_t length = strlen(buffer);
    if (length == 0) {
        return;
    }
    --length;
    while (length > 0 && ((unsigned char)buffer[length] & 0xC0U) == 0x80U) {
        --length;
    }
    buffer[length] = '\0';
}

static void draw_window_bar(shell_state *const state)
{
    struct ncplane *const plane = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_WINDOW);
    if (plane == nullptr) {
        return;
    }
    for (size_t index = 0; index < APL_TUI_MENU_GROUP_COUNT; ++index) {
        const apl_tui_menu_group group = (apl_tui_menu_group)index;
        const bool focused = state->focus.region == APL_TUI_FOCUS_WINDOW &&
                             (state->menu.open ? state->menu.group == group
                                               : state->menu.group_focus == group);
        apl_tui_frame_set_styles(plane, focused ? NCSTYLE_BOLD : 0);
        apl_tui_frame_draw_text(plane, 0, (int)apl_tui_menu_heading_offset(group),
                                apl_tui_menu_group_label(group));
    }
    apl_tui_frame_set_styles(plane, 0);
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
        apl_tui_frame_set_styles(popup, item->enabled && focused ? NCSTYLE_BOLD : 0);
        apl_tui_frame_draw_text(popup, (int)(index + 1), 1, line);
    }
    apl_tui_frame_set_styles(popup, 0);
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
        apl_tui_frame_set_styles(body, focused ? NCSTYLE_BOLD : 0);
        apl_tui_frame_draw_line(body, (int)offset, line);
        apl_tui_frame_set_styles(body, 0);
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

static void draw_team_form(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }

    apl_tui_frame_draw_line(body, 0, state->form.editing ? "Edit team" : "Create a new team");

    char line[APL_TEAM_NAME_MAX + APL_TEAM_NOTES_MAX + 16];
    int row = 2;
    (void)snprintf(line, sizeof(line), "%c Name (required): %s", state->form.focus == 0 ? '>' : ' ',
                   state->form.name);
    apl_tui_frame_set_styles(body, state->form.focus == 0 ? NCSTYLE_BOLD : 0);
    apl_tui_frame_draw_line(body, row++, line);
    apl_tui_frame_set_styles(body, 0);

    (void)snprintf(line, sizeof(line), "%c Notes (optional): %s",
                   state->form.focus == 1 ? '>' : ' ', state->form.notes);
    apl_tui_frame_set_styles(body, state->form.focus == 1 ? NCSTYLE_BOLD : 0);
    apl_tui_frame_draw_line(body, row++, line);
    apl_tui_frame_set_styles(body, 0);

    apl_tui_frame_draw_text(body, 5, 2, "[ Save ]");
    apl_tui_frame_draw_text(body, 5, 13, "[ Cancel ]");
    apl_tui_frame_draw_line(body, 6, "Tab next field   Enter save   Esc cancel");
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
    apl_tui_frame_draw_line(body, 3, "Use [ and ] to return to Teams.");
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

    apl_tui_frame_draw_text(overlay, 1, 2, state->dialog.title);
    const unsigned message_rows = apl_tui_frame_draw_wrapped(
        overlay, 3, 2, cols > 4 ? cols - 4 : cols, state->dialog.message);
    (void)message_rows;

    const int button_row = (int)rows - 2;
    if (state->dialog.kind == APL_TUI_DIALOG_CONFIRM) {
        apl_tui_frame_set_styles(overlay, state->dialog.focus == 0 ? NCSTYLE_BOLD : 0);
        apl_tui_frame_draw_text(overlay, button_row, 2, "[ Cancel ]");
        apl_tui_frame_set_styles(overlay, state->dialog.focus == 1 ? NCSTYLE_BOLD : 0);
        apl_tui_frame_draw_text(overlay, button_row, 15, "[ Confirm ]");
        apl_tui_frame_set_styles(overlay, 0);
    } else {
        apl_tui_frame_set_styles(overlay, NCSTYLE_BOLD);
        apl_tui_frame_draw_text(overlay, button_row, 2, "[ OK ]");
        apl_tui_frame_set_styles(overlay, 0);
    }
}

static void draw_screen(shell_state *const state)
{
    draw_window_bar(state);

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
        case SHELL_SCREEN_TEAM_FORM:
            draw_team_form(state);
            break;
        case SHELL_SCREEN_PLACEHOLDER:
            draw_placeholder(state);
            break;
        }
    }

    struct ncplane *const status = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_STATUS);
    if (status != nullptr && state->status[0] != '\0') {
        apl_tui_frame_draw_line(status, 0, state->status);
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
    if (layout == nullptr) {
        return;
    }
    const apl_tui_rect overlay = layout->overlay;
    const int button_row = (int)(overlay.y + overlay.rows) - 2;
    if (input->y != button_row) {
        return;
    }

    if (state->dialog.kind == APL_TUI_DIALOG_ERROR) {
        if (input->x >= (int)overlay.x + 2 && input->x < (int)overlay.x + 8) {
            ncinput escape = {.id = NCKEY_ESC};
            handle_dialog_input(state, &escape);
        }
        return;
    }
    if (state->dialog.kind != APL_TUI_DIALOG_CONFIRM) {
        return;
    }

    ncinput enter = {.id = NCKEY_ENTER};
    if (input->x >= (int)overlay.x + 2 && input->x < (int)overlay.x + 12) {
        state->dialog.focus = 0;
        handle_dialog_input(state, &enter);
    } else if (input->x >= (int)overlay.x + 15 && input->x < (int)overlay.x + 26) {
        state->dialog.focus = 1;
        handle_dialog_input(state, &enter);
    }
}

static void handle_form_mouse(shell_state *const state, const ncinput *const input)
{
    const apl_tui_layout *const layout = apl_tui_frame_layout(&state->frame);
    if (layout == nullptr || !point_in_rect(layout->body, input->y, input->x)) {
        return;
    }
    const unsigned row = (unsigned)(input->y - (int)layout->body.y);

    if (row == 2) {
        state->form.focus = 0;
        return;
    }
    if (row == 3) {
        state->form.focus = 1;
        return;
    }
    if (row != 5) {
        return;
    }

    const unsigned col = (unsigned)(input->x - (int)layout->body.x);
    if (col >= 2 && col < 10) {
        apl_error error;
        apl_error_init(&error);
        if (state->form.editing) {
            perform_update(state, &error);
        } else {
            perform_create(state, &error);
        }
    } else if (col >= 13 && col < 23) {
        if (form_is_dirty(state)) {
            state->pending = SHELL_PENDING_DISCARD_FORM;
            apl_tui_dialog_open_confirm(&state->dialog, "Discard changes?",
                                        "The values you typed will be lost.");
        } else {
            close_form(state);
        }
    }
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
    case SHELL_SCREEN_TEAM_FORM:
        if (input->id == NCKEY_BUTTON1) {
            handle_form_mouse(state, input);
        }
        break;
    default:
        break;
    }
}

static void handle_dialog_input(shell_state *const state, const ncinput *const input)
{
    const uint32_t id = input->id;
    if (id == NCKEY_LEFT) {
        apl_tui_dialog_move_focus(&state->dialog, -1);
        return;
    }
    if (id == NCKEY_RIGHT) {
        apl_tui_dialog_move_focus(&state->dialog, 1);
        return;
    }
    if (id == NCKEY_TAB) {
        apl_tui_dialog_move_focus(&state->dialog, ncinput_shift_p(input) ? -1 : 1);
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
    const shell_pending_action pending = state->pending;
    const int64_t pending_team = state->pending_team_id;
    state->pending = SHELL_PENDING_NONE;
    state->pending_team_id = 0;
    apl_tui_dialog_init(&state->dialog);
    apl_tui_focus_pop(&state->focus);

    if (pending == SHELL_PENDING_DISCARD_FORM) {
        if (result == APL_TUI_DIALOG_ACCEPTED) {
            close_form(state);
            set_status(state, "Changes discarded.");
        } else {
            set_status(state, "Kept editing.");
        }
    } else if (pending == SHELL_PENDING_ARCHIVE_TEAM) {
        if (result == APL_TUI_DIALOG_ACCEPTED) {
            perform_archive(state, pending_team, &error);
        } else {
            set_status(state, "Archive cancelled.");
        }
    }
}

static void handle_form_input(shell_state *const state, const ncinput *const input)
{
    const uint32_t id = input->id;
    if (id == NCKEY_ESC) {
        if (form_is_dirty(state)) {
            state->pending = SHELL_PENDING_DISCARD_FORM;
            apl_tui_dialog_open_confirm(&state->dialog, "Discard changes?",
                                        "The values you typed will be lost.");
            return;
        }
        close_form(state);
        return;
    }
    if (id == NCKEY_TAB) {
        if (ncinput_shift_p(input)) {
            state->form.focus = (state->form.focus == 0) ? 1 : 0;
        } else {
            state->form.focus = (state->form.focus + 1) % 2;
        }
        return;
    }
    if (is_enter_key(id)) {
        apl_error error;
        apl_error_init(&error);
        if (state->form.editing) {
            perform_update(state, &error);
        } else {
            perform_create(state, &error);
        }
        return;
    }
    if (id == NCKEY_BACKSPACE || id == 127) {
        if (state->form.focus == 0) {
            backspace_utf8(state->form.name);
        } else {
            backspace_utf8(state->form.notes);
        }
        return;
    }

    if (id >= 0x20 && id < PRETERUNICODEBASE && input->utf8[0] != '\0') {
        if (state->form.focus == 0) {
            append_utf8(state->form.name, sizeof(state->form.name), input->utf8);
        } else {
            append_utf8(state->form.notes, sizeof(state->form.notes), input->utf8);
        }
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
    case 'S': {
        const apl_team *const team = selected_team(state);
        if (team != nullptr) {
            if (team->archived) {
                set_status(state, "Archived teams cannot be active; choose an active team.");
            } else {
                perform_set_active(state, team->id, &error);
                if (!apl_error_is_success(&error)) {
                    show_error_dialog(state, &error);
                }
            }
        }
        return;
    }
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
    apl_error error;
    apl_error_init(&error);

    switch (id) {
    case NCKEY_ESC:
        state->screen = state->any_teams ? SHELL_SCREEN_TEAM_LIST : SHELL_SCREEN_WELCOME;
        return;
    case 'e':
    case 'E':
        if (state->detail.archived) {
            set_status(state, "Archived teams cannot be edited in this phase.");
        } else {
            open_edit_form(state);
        }
        return;
    case 's':
    case 'S':
        if (state->detail.archived) {
            set_status(state, "Archived teams cannot be active; choose an active team.");
        } else {
            perform_set_active(state, state->detail.id, &error);
            if (!apl_error_is_success(&error)) {
                show_error_dialog(state, &error);
            }
        }
        return;
    case 'x':
    case 'X':
        if (state->detail.archived) {
            set_status(state, "This team is already archived.");
        } else if (!can_archive_team(state, &state->detail)) {
            set_status(state,
                       "The last active team cannot be archived. Create another team first.");
        } else {
            state->pending = SHELL_PENDING_ARCHIVE_TEAM;
            state->pending_team_id = state->detail.id;
            apl_tui_dialog_open_confirm(&state->dialog, "Archive team?",
                                        "The team will be hidden from the active list. Its "
                                        "records and history are kept.");
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
            apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_BODY);
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
    case NCKEY_TAB:
        apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_BODY);
        return;
    default:
        break;
    }
    if (is_enter_key(id)) {
        apl_tui_menu_open(&state->menu, state->menu.group_focus);
    }
}

static void handle_input(shell_state *const state, const ncinput *const input)
{
    const uint32_t id = input->id;
    if (apl_tui_dialog_active(&state->dialog)) {
        handle_dialog_input(state, input);
        return;
    }
    if (id == NCKEY_F10) {
        apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_WINDOW);
        return;
    }
    if ((id == 'm' || id == 'M') &&
        !(state->section == APL_TUI_SECTION_TEAMS && state->screen == SHELL_SCREEN_TEAM_FORM)) {
        apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_WINDOW);
        apl_tui_menu_open(&state->menu, state->menu.group_focus);
        return;
    }
    if (state->focus.region == APL_TUI_FOCUS_WINDOW) {
        handle_window_input(state, input);
        return;
    }
    if (id == NCKEY_TAB &&
        !(state->section == APL_TUI_SECTION_TEAMS && state->screen == SHELL_SCREEN_TEAM_FORM)) {
        apl_tui_menu_close(&state->menu);
        apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_WINDOW);
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
    case SHELL_SCREEN_TEAM_FORM:
        handle_form_input(state, input);
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
    apl_tui_dialog_init(&state->dialog);
    apl_tui_menu_init(&state->menu);
    apl_tui_focus_init(&state->focus);

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
        set_status(state, "Active team: %s", state->active_team.name);
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
    if (status == APL_OK) {
        apl_error_clear(error);
    }
    return status;
}
