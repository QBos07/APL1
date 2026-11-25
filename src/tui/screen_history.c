#include "tui/shell_internal.h"

const char *history_entity_filter_name(const unsigned index)
{
    static const apl_record_kind KINDS[] = {
        APL_RECORD_NONE,           APL_RECORD_TEAM,        APL_RECORD_SEASON,
        APL_RECORD_DRIVER,         APL_RECORD_MEMBERSHIP,  APL_RECORD_CAR,
        APL_RECORD_LEDGER_ENTRY,   APL_RECORD_BUDGET_PLAN, APL_RECORD_PROJECT,
        APL_RECORD_PROJECT_UPDATE,
    };
    return (index < APL_COUNT_OF(KINDS)) ? apl_record_kind_code(KINDS[index]) : "";
}

const char *history_action_filter_name(const unsigned index)
{
    static const char *const NAMES[] = {
        "",
        "team_created",
        "team_renamed",
        "team_archived",
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
        "ledger_opening_balance",
        "ledger_income",
        "ledger_expense",
        "ledger_corrected",
        "budget_created",
        "budget_updated",
        "project_created",
        "project_updated",
        "project_archived",
        "project_update_logged",
        "project_status_changed",
    };
    return (index < APL_COUNT_OF(NAMES)) ? NAMES[index] : "";
}

unsigned history_entity_filter_count(void)
{
    unsigned count = 1; /* "all" plus every explicit record kind */
    while (history_entity_filter_name(count)[0] != '\0') {
        ++count;
    }
    return count;
}

unsigned history_action_filter_count(void)
{
    unsigned count = 1; /* "all" plus every explicit action */
    while (history_action_filter_name(count)[0] != '\0') {
        ++count;
    }
    return count;
}

apl_status load_history(shell_state *const state, apl_error *const error)
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
    /* Older development databases may contain team_selected rows written by a
     * previous build. Switching the active team is intentionally not team
     * activity, so keep those rows out of the visible history. */
    size_t kept = 0;
    for (size_t index = 0; index < count; ++index) {
        if (strcmp(events[index].action, "team_selected") != 0) {
            events[kept++] = events[index];
        }
    }
    count = kept;

    apl_activity_service_free_events(state->events);
    state->events = events;
    state->event_count = count;
    apl_tui_list_set_count(&state->list, count);
    return APL_OK;
}

const char *history_entity_filter_short_name(const unsigned index)
{
    static const char *const NAMES[] = {"all", "team",  "season", "driver",  "member",
                                        "car", "money", "budget", "project", "update"};
    return (index < APL_COUNT_OF(NAMES)) ? NAMES[index] : "all";
}

const char *history_action_filter_short_name(const unsigned index)
{
    static const char *const NAMES[] = {
        "all",
        "team:created",
        "team:renamed",
        "team:archived",
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
        "money:opening",
        "money:income",
        "money:expense",
        "money:corrected",
        "budget:created",
        "budget:updated",
        "project:created",
        "project:updated",
        "project:archived",
        "project:update",
        "project:status",
    };
    return (index < APL_COUNT_OF(NAMES)) ? NAMES[index] : "all";
}

const char *history_date_filter_short_name(const unsigned preset)
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

void append_history_filter_button(char *const out, const size_t out_size, unsigned *const cursor,
                                  unsigned starts[5], unsigned ends[5], const unsigned index,
                                  const char *const label)
{
    starts[index] = *cursor;
    (void)apl_tui_text_append(out, out_size - *cursor, label);
    *cursor = (unsigned)strlen(out);
    ends[index] = *cursor;
    (void)apl_tui_text_append(out, out_size - *cursor, "  ");
    *cursor = (unsigned)strlen(out);
}

void build_history_filter_bar(const shell_state *const state, char *const out,
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

void fit_text_with_ellipsis(char *const out, const size_t out_size, const char *const text,
                            const size_t width)
{
    if (out == nullptr || out_size == 0) {
        return;
    }
    out[0] = '\0';
    if (text == nullptr || width == 0) {
        return;
    }
    const size_t copy_limit = (width < out_size) ? width : out_size - 1;
    (void)snprintf(out, out_size, "%s", text);
    if (strlen(out) <= copy_limit) {
        return;
    }
    if (copy_limit <= 3) {
        (void)apl_tui_text_truncate_utf8(out, copy_limit);
        return;
    }
    (void)apl_tui_text_truncate_utf8(out, copy_limit - 3);
    (void)apl_tui_text_append(out, out_size, "...");
}

void draw_history_list(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    const unsigned cols = apl_tui_frame_layout(&state->frame)->body.cols;
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
    constexpr size_t ACTION_WIDTH = 22;
    constexpr size_t PREFIX_WIDTH = 1 + 1 + 16 + 2 + ACTION_WIDTH + 1;
    const size_t summary_width = (cols > PREFIX_WIDTH) ? cols - PREFIX_WIDTH : 0;
    const size_t visible = apl_tui_list_visible_count(&state->list);
    for (size_t offset = 0; offset < visible && offset + 3 < rows - 1; ++offset) {
        const size_t index = state->list.offset + offset;
        const apl_activity_event *const event = &state->events[index];
        const bool focused = index == state->list.selected;
        char stamp[17];
        (void)snprintf(stamp, sizeof(stamp), "%.16s", event->occurred_at);
        char action[ACTION_WIDTH + 1];
        fit_text_with_ellipsis(action, sizeof(action), event->action, ACTION_WIDTH);
        char summary[APL_ACTIVITY_SUMMARY_MAX];
        fit_text_with_ellipsis(summary, sizeof(summary), event->summary, summary_width);
        char line[APL_ACTIVITY_SUMMARY_MAX + 160];
        (void)snprintf(line, sizeof(line), "%c %s  %-*s %s", focused ? '>' : ' ', stamp,
                       (int)ACTION_WIDTH, action, summary);
        apl_tui_theme_apply(&state->theme, body,
                            focused ? APL_TUI_ROLE_SELECTION : APL_TUI_ROLE_BODY);
        apl_tui_frame_draw_line(body, (int)(offset + 3), line);
        apl_tui_theme_reset(body);
    }
}

void reload_history_with_status(shell_state *const state, const char *const message)
{
    apl_error error;
    apl_error_init(&error);
    if (load_history(state, &error) != APL_OK) {
        show_error_dialog(state, &error);
        return;
    }
    set_status(state, "%s", message);
}

void handle_history_input(shell_state *const state, const uint32_t id)
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
        state->history_entity_filter =
            (state->history_entity_filter + 1) % history_entity_filter_count();
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
