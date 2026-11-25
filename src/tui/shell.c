#include "tui/shell.h"

#include <limits.h>
#include <notcurses/notcurses.h>
#include <stdarg.h>
#include <stdckdint.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "app/activity_service.h"
#include "app/budget_service.h"
#include "app/car_service.h"
#include "app/driver_service.h"
#include "app/ledger_service.h"
#include "app/membership_service.h"
#include "app/project_service.h"
#include "app/season_service.h"
#include "app/team_service.h"
#include "app/version.h"
#include "core/util.h"
#include "domain/ledger.h"
#include "domain/money.h"
#include "domain/record.h"
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

#include "tui/shell_internal.h"

static void open_current_list_record(shell_state *state);
static void handle_input(shell_state *state, const ncinput *input);

void set_section(shell_state *const state, const apl_tui_section section)
{
    state->project_return_pending = false;
    const apl_tui_section previous = state->section;
    shell_save_view_state(state, previous);
    state->section = section;
    shell_restore_view_state(state, section);
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
    case APL_TUI_SECTION_MONEY:
        state->screen = SHELL_SCREEN_MONEY_LIST;
        if (load_ledger(state, 0, &error) != APL_OK) {
            show_error_dialog(state, &error);
        }
        break;
    case APL_TUI_SECTION_DEVELOPMENT:
        state->screen = SHELL_SCREEN_PROJECT_LIST;
        if (load_projects(state, 0, &error) != APL_OK) {
            show_error_dialog(state, &error);
        }
        break;
    default:
        state->screen = SHELL_SCREEN_PLACEHOLDER;
        break;
    }
    sync_status_context(state);
}
void switch_section(shell_state *const state, const bool forward)
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
    case APL_TUI_MENU_ACTION_OPEN_MONEY: {
        set_section(state, APL_TUI_SECTION_MONEY);
        set_status(state, "Money ledger for the active team.");
        return;
    }
    case APL_TUI_MENU_ACTION_OPEN_BUDGETS: {
        apl_error error;
        apl_error_init(&error);
        state->section = APL_TUI_SECTION_MONEY;
        state->screen = SHELL_SCREEN_BUDGET_LIST;
        if (load_budget_report(state, &error) != APL_OK) {
            show_error_dialog(state, &error);
        } else {
            sync_status_context(state);
            set_status(state, "Budget report: planned versus recorded actual spending.");
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

static unsigned quit_button_start(const unsigned columns)
{
    const unsigned keys = keys_button_start(columns);
    return (keys > 7) ? keys - 7 : keys;
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
    if (layout->window.cols > 55) {
        apl_tui_theme_apply(&state->theme, plane, APL_TUI_ROLE_WINDOW_FOCUS);
        apl_tui_frame_draw_text(plane, 0, (int)quit_button_start(layout->window.cols), "[Quit]");
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
        const bool focused = state->focus.region == APL_TUI_FOCUS_TABS && !state->team_focus &&
                             index == state->tabs.selected;
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
    if (state->section == APL_TUI_SECTION_DEVELOPMENT) {
        if (state->screen == SHELL_SCREEN_PROJECT_DETAIL) {
            return "u update | e edit | x archive | b budgets | Enter cost | Esc projects";
        }
        return "n new | Enter open | f filters | a archived | c clear";
    }
    if (state->section == APL_TUI_SECTION_MONEY) {
        if (state->screen == SHELL_SCREEN_MONEY_DETAIL) {
            return "c correct | Esc ledger";
        }
        if (state->screen == SHELL_SCREEN_BUDGET_LIST) {
            return "n plan | Enter edit | d dates | F category | c clear | Esc ledger";
        }
        return "i income | x expense | o opening | u currency | b budgets | d dates | g kind | "
               "F category | c clear";
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
        return "e edit | x archive | o seasons | c cars | p people | f money | s switch";
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
    case SHELL_SCREEN_MONEY_LIST:
    case SHELL_SCREEN_MONEY_DETAIL:
    case SHELL_SCREEN_BUDGET_LIST:
    case SHELL_SCREEN_PROJECT_LIST:
    case SHELL_SCREEN_PROJECT_DETAIL:
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
    const bool focused = state->focus.region == APL_TUI_FOCUS_BODY;
    (void)snprintf(line, sizeof(line), "%sKeys: %s", focused ? "Body | " : "",
                   context_key_hint(state));
    apl_tui_theme_apply(&state->theme, body, focused ? APL_TUI_ROLE_TAB_FOCUS : APL_TUI_ROLE_MUTED);
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

static void draw_dialog_message_line(struct ncplane *const overlay, const int row,
                                     const unsigned cols, const char *const message)
{
    if (overlay == nullptr || message == nullptr || cols <= 5) {
        return;
    }
    char line[APL_TUI_DIALOG_MESSAGE_MAX];
    (void)snprintf(line, sizeof(line), "%.*s", (int)(cols - 5), message);
    apl_tui_frame_draw_text(overlay, row, 2, line);
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
        draw_dialog_message_line(overlay, 2, cols, state->dialog.message);
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
    } else if (state->section == APL_TUI_SECTION_DEVELOPMENT) {
        if (state->screen == SHELL_SCREEN_PROJECT_DETAIL) {
            draw_project_detail(state);
        } else {
            draw_project_list(state);
        }
    } else if (state->section == APL_TUI_SECTION_MONEY) {
        if (state->screen == SHELL_SCREEN_MONEY_DETAIL) {
            draw_money_detail(state);
        } else if (state->screen == SHELL_SCREEN_BUDGET_LIST) {
            draw_budget_list(state);
        } else {
            draw_money_list(state);
        }
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
        case SHELL_SCREEN_MONEY_LIST:
        case SHELL_SCREEN_MONEY_DETAIL:
        case SHELL_SCREEN_BUDGET_LIST:
        case SHELL_SCREEN_PROJECT_LIST:
        case SHELL_SCREEN_PROJECT_DETAIL:
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
            if (state->focus.region == APL_TUI_FOCUS_STATUS) {
                role = APL_TUI_ROLE_TAB_FOCUS;
            } else if (state->status.message[0] != '\0') {
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
        if (chosen.meta.archived) {
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
    } else if (state->section == APL_TUI_SECTION_MONEY) {
        count = (state->screen == SHELL_SCREEN_BUDGET_LIST) ? state->budget_row_count
                                                            : state->entry_count;
    } else if (state->section == APL_TUI_SECTION_DEVELOPMENT) {
        if (state->screen == SHELL_SCREEN_PROJECT_DETAIL) {
            return;
        }
        count = state->project_count;
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
    if (state->section == APL_TUI_SECTION_MONEY) {
        if (state->screen == SHELL_SCREEN_MONEY_DETAIL) {
            return;
        }
        if (row == 1) {
            unsigned starts[4] = {};
            unsigned ends[4] = {};
            char filters[320];
            const bool budgets = state->screen == SHELL_SCREEN_BUDGET_LIST;
            if (budgets) {
                build_budget_filter_bar(state, filters, sizeof(filters), starts, ends);
            } else {
                build_money_filter_bar(state, filters, sizeof(filters), starts, ends);
            }
            const unsigned column = (unsigned)(input->x - (int)layout->body.x);
            const uint32_t ledger_keys[] = {'d', 'g', 'F', 'C'};
            const uint32_t budget_keys[] = {'d', 'F', 'C'};
            const unsigned button_count = budgets ? 3 : 4;
            for (unsigned button = 0; button < button_count; ++button) {
                if (column >= starts[button] && column < ends[button] + 2) {
                    const uint32_t key = budgets ? budget_keys[button] : ledger_keys[button];
                    if (budgets) {
                        handle_budget_list_input(state, key);
                    } else {
                        handle_money_list_input(state, key);
                    }
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
    if (state->section == APL_TUI_SECTION_DEVELOPMENT) {
        if (row == 1) {
            unsigned starts[5] = {};
            unsigned ends[5] = {};
            char filters[512];
            build_project_filter_bar(state, filters, sizeof(filters), starts, ends);
            const unsigned column = (unsigned)(input->x - (int)layout->body.x);
            const uint32_t project_keys[] = {'f', 'f', 'f', 'a', 'C'};
            for (unsigned button = 0; button < 5; ++button) {
                if (column >= starts[button] && column < ends[button] + 2) {
                    handle_project_list_input(state, project_keys[button]);
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
    const bool was_selected = index == state->list.selected;
    apl_tui_list_select(&state->list, index);
    if (was_selected) {
        state->last_click_team_id = 0;
        state->last_click_ms = 0;
        open_current_list_record(state);
        return;
    }

    if (state->section == APL_TUI_SECTION_PEOPLE) {
        record_id = state->drivers[index].id;
    } else if (state->section == APL_TUI_SECTION_HISTORY) {
        record_id = state->events[index].id;
    } else if (state->section == APL_TUI_SECTION_MONEY) {
        record_id = (state->screen == SHELL_SCREEN_BUDGET_LIST) ? (int64_t)index + 1
                                                                : state->entries[index].id;
    } else if (state->section == APL_TUI_SECTION_DEVELOPMENT) {
        record_id = state->projects[index].id;
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
        open_current_list_record(state);
    } else {
        state->last_click_team_id = record_id;
        state->last_click_ms = current;
    }
}

/** Make the persistent key-hint row clickable, using the same key handlers as
 *  the keyboard. This gives mouse access to actions such as Enter/open,
 *  membership add/end, edit, archive, and view switching. */
static bool handle_key_hint_mouse(shell_state *const state, const apl_tui_layout *const layout,
                                  const ncinput *const input)
{
    if (input->id != NCKEY_BUTTON1 || input->y != (int)(layout->body.y + layout->body.rows - 1)) {
        return false;
    }
    const unsigned column = (unsigned)(input->x - (int)layout->body.x);
    char line[400];
    (void)snprintf(line, sizeof(line), "Keys: %s", context_key_hint(state));
    const size_t length = strlen(line);
    if (column >= length) {
        return true;
    }
    const char *const prefix = "Keys: ";
    size_t position = strlen(prefix);
    while (position <= length) {
        size_t end = position;
        while (end < length && strncmp(line + end, " | ", 3) != 0) {
            ++end;
        }
        if (column >= position && column <= end) {
            const char *segment = line + position;
            while (*segment == ' ') {
                ++segment;
            }
            uint32_t key = 0;
            if (strncmp(segment, "Enter ", 6) == 0) {
                key = NCKEY_ENTER;
            } else if (strncmp(segment, "Esc ", 4) == 0) {
                key = NCKEY_ESC;
            } else if (strncmp(segment, ": commands", 10) == 0) {
                key = ':';
            } else if (segment[0] == ',') {
                key = ',';
            } else {
                key = (uint32_t)(unsigned char)segment[0];
            }
            apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_BODY);
            const ncinput synthesized = {.id = key};
            handle_input(state, &synthesized);
            return true;
        }
        if (end >= length) {
            break;
        }
        position = end + 3;
    }
    return true;
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
        apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_BODY);
        state->team_focus = false;
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
        } else if (layout->window.cols > 55 &&
                   (unsigned)input->x >= quit_button_start(layout->window.cols)) {
            if ((unsigned)input->x < keys_button_start(layout->window.cols)) {
                state->quit = true;
            } else {
                handle_menu_action(state, APL_TUI_MENU_ACTION_HELP_KEYBOARD);
            }
        }
        return;
    }

    if (input->id == NCKEY_BUTTON1 && layout != nullptr &&
        point_in_rect(layout->body, input->y, input->x)) {
        apl_tui_focus_set(&state->focus, APL_TUI_FOCUS_BODY);
        state->team_focus = false;
        if (handle_key_hint_mouse(state, layout, input)) {
            return;
        }
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
    if (state->section == APL_TUI_SECTION_MONEY) {
        if (state->screen == SHELL_SCREEN_MONEY_LIST || state->screen == SHELL_SCREEN_BUDGET_LIST) {
            handle_list_mouse(state, input);
        }
        return;
    }
    if (state->section == APL_TUI_SECTION_DEVELOPMENT) {
        if (state->screen == SHELL_SCREEN_PROJECT_LIST) {
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

static void open_current_list_record(shell_state *const state)
{
    if (state->section == APL_TUI_SECTION_PEOPLE) {
        open_selected_person(state);
    } else if (state->section == APL_TUI_SECTION_HISTORY) {
        handle_history_input(state, NCKEY_ENTER);
    } else if (state->section == APL_TUI_SECTION_MONEY) {
        if (state->screen == SHELL_SCREEN_MONEY_DETAIL) {
            return;
        }
        if (state->screen == SHELL_SCREEN_BUDGET_LIST) {
            open_selected_budget(state);
        } else {
            open_selected_money_entry(state);
        }
    } else if (state->section == APL_TUI_SECTION_DEVELOPMENT) {
        open_selected_project(state);
    } else if (state->screen == SHELL_SCREEN_SEASON_LIST) {
        open_selected_season(state);
    } else if (state->screen == SHELL_SCREEN_CAR_LIST) {
        open_selected_car(state);
    } else {
        open_selected_detail(state);
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
    if (state->section == APL_TUI_SECTION_DEVELOPMENT) {
        if (state->screen == SHELL_SCREEN_PROJECT_DETAIL) {
            handle_project_detail_input(state, id);
        } else {
            handle_project_list_input(state, id);
        }
        return;
    }
    if (state->section == APL_TUI_SECTION_MONEY) {
        if (state->screen == SHELL_SCREEN_MONEY_DETAIL) {
            handle_money_detail_input(state, id);
        } else if (state->screen == SHELL_SCREEN_BUDGET_LIST) {
            handle_budget_list_input(state, id);
        } else {
            handle_money_list_input(state, id);
        }
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
    case SHELL_SCREEN_MONEY_LIST:
    case SHELL_SCREEN_MONEY_DETAIL:
    case SHELL_SCREEN_BUDGET_LIST:
    case SHELL_SCREEN_PROJECT_LIST:
    case SHELL_SCREEN_PROJECT_DETAIL:
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

apl_status apl_tui_shell_run(const apl_tui *const tui, const apl_app *const app,
                             apl_error *const error)
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
            if (state.section == APL_TUI_SECTION_HISTORY ||
                (state.section == APL_TUI_SECTION_MONEY &&
                 state.screen != SHELL_SCREEN_MONEY_DETAIL)) {
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
