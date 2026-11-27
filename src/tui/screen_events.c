#include "tui/shell_internal.h"

apl_status load_race_events(shell_state *state, apl_error *error)
{
    apl_event *events = nullptr;
    size_t count = 0;
    int64_t team = active_team_id(state);
    apl_status status = APL_OK;
    if (team > 0)
        status = apl_event_repository_list(&state->app->db, team, &events, &count, error);
    if (status != APL_OK)
        return status;
    apl_event_repository_free_list(state->race_events);
    state->race_events = events;
    state->race_event_count = count;
    apl_tui_list_set_count(&state->list, count);
    return APL_OK;
}

static void line_value(char *out, size_t size, bool present, int value)
{
    if (present)
        (void)snprintf(out, size, "%d", value);
    else
        (void)snprintf(out, size, "not recorded");
}

static const char *text_at_offset(const char *text, size_t offset)
{
    const unsigned char *p = (const unsigned char *)text;
    while (*p && offset) {
        ++p;
        while ((*p & 0xc0) == 0x80)
            ++p;
        --offset;
    }
    return (const char *)p;
}

static void draw_history_values(struct ncplane *body, int row, const char *label,
                                const apl_outcome *value, size_t offset)
{
    char start[32], finish[32], laps[32], text[1024];
    line_value(start, sizeof(start), value->has_start_position, value->start_position);
    line_value(finish, sizeof(finish), value->has_finish_position, value->finish_position);
    line_value(laps, sizeof(laps), value->has_laps, value->laps);
    (void)snprintf(text, sizeof(text), "%s  Start: %s  Finish: %s  Laps: %s", label, start, finish,
                   laps);
    apl_tui_frame_draw_line(body, row, text);
    (void)snprintf(text, sizeof(text), "  Status: %s",
                   value->has_status ? text_at_offset(value->status, offset) : "not recorded");
    apl_tui_frame_draw_line(body, row + 1, text);
    (void)snprintf(text, sizeof(text), "  Notes: %.900s",
                   value->has_notes ? text_at_offset(value->notes, offset) : "not recorded");
    apl_tui_frame_draw_line(body, row + 2, text);
}

void draw_events_screen(shell_state *state)
{
    struct ncplane *body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (!body)
        return;
    const apl_tui_layout *layout = apl_tui_frame_layout(&state->frame);
    unsigned rows = layout->body.rows;
    char line[1024];
    if (active_team_id(state) == 0) {
        apl_tui_frame_draw_line(body, 0, "No active team. Select a team to review events.");
        return;
    }
    if (state->screen == SHELL_SCREEN_OUTCOME_HISTORY) {
        const apl_session_outcome_row *row =
            &state->race_session_detail.rows[state->outcome_return_index];
        (void)snprintf(line, sizeof(line), "Team: %s | Event: %s | Session: %s",
                       state->active_team.name, state->race_session_detail.event.name,
                       state->race_session_detail.session.label);
        apl_tui_frame_draw_line(body, 0, line);
        (void)snprintf(line, sizeof(line), "Driver: %s | text offset: %zu", row->driver.name,
                       state->outcome_history_offset);
        apl_tui_frame_draw_line(body, 1, line);
        if (state->outcome_correction_count == 0) {
            apl_tui_frame_draw_line(body, 2, "No corrections recorded for this outcome.");
            draw_history_values(body, 4, "Current:", &row->outcome, state->outcome_history_offset);
            return;
        }
        const apl_outcome_correction *c = &state->outcome_corrections[state->list.selected];
        (void)snprintf(line, sizeof(line), "Correction %zu/%zu | %s | %s", state->list.selected + 1,
                       state->outcome_correction_count, c->occurred_at, c->actor);
        apl_tui_frame_draw_line(body, 2, line);
        draw_history_values(body, 4, state->outcome_history_after ? "After:" : "Before:",
                            state->outcome_history_after ? &c->after : &c->before,
                            state->outcome_history_offset);
        return;
    }
    unsigned start;
    size_t count;
    if (state->screen == SHELL_SCREEN_EVENT_LIST) {
        (void)snprintf(line, sizeof(line), "Events | Team: %s | %zu recorded",
                       state->active_team.name, state->race_event_count);
        apl_tui_frame_draw_line(body, 0, line);
        start = 2;
        count = state->race_event_count;
        if (!count)
            apl_tui_frame_draw_line(body, 2, "No events recorded.");
    } else if (state->screen == SHELL_SCREEN_EVENT_DETAIL) {
        const apl_event *event = &state->race_event_detail.event;
        (void)snprintf(line, sizeof(line), "Team: %s | Event: %s", state->active_team.name,
                       event->name);
        apl_tui_frame_draw_line(body, 0, line);
        char season[64];
        if (event->season_id > 0)
            (void)snprintf(season, sizeof(season), "%lld", (long long)event->season_id);
        else
            (void)snprintf(season, sizeof(season), "not linked");
        (void)snprintf(line, sizeof(line), "Date: %s | Location: %s | Season ID: %s", event->date,
                       event->location[0] ? event->location : "not recorded", season);
        apl_tui_frame_draw_line(body, 1, line);
        (void)snprintf(line, sizeof(line), "Notes: %.1000s",
                       event->notes[0] ? event->notes : "not recorded");
        apl_tui_frame_draw_line(body, 2, line);
        apl_tui_frame_draw_line(body, 3, "Sessions (explicitly recorded):");
        start = 4;
        count = state->race_event_detail.session_count;
        if (!count)
            apl_tui_frame_draw_line(body, 4, "No sessions recorded.");
    } else {
        (void)snprintf(line, sizeof(line), "Team: %s | Event: %s | Session: %s",
                       state->active_team.name, state->race_session_detail.event.name,
                       state->race_session_detail.session.label);
        apl_tui_frame_draw_line(body, 0, line);
        apl_tui_frame_draw_line(body, 1, "Team driver directory (not a participation list)");
        start = 3;
        count = state->race_session_detail.row_count;
        if (!count)
            apl_tui_frame_draw_line(body, 3, "No team drivers recorded.");
    }
    apl_tui_list_set_viewport_rows(&state->list, rows > start + 1 ? rows - start - 1 : 1);
    for (size_t i = 0; i < apl_tui_list_visible_count(&state->list); ++i) {
        size_t index = state->list.offset + i;
        if (index >= count || start + i >= rows - 1)
            break;
        bool selected = index == state->list.selected;
        if (state->screen == SHELL_SCREEN_EVENT_LIST) {
            const apl_event *event = &state->race_events[index];
            (void)snprintf(line, sizeof(line), "%c %s  %s", selected ? '>' : ' ', event->date,
                           event->name);
        } else if (state->screen == SHELL_SCREEN_EVENT_DETAIL) {
            (void)snprintf(line, sizeof(line), "%c %s", selected ? '>' : ' ',
                           state->race_event_detail.sessions[index].label);
        } else {
            const apl_session_outcome_row *r = &state->race_session_detail.rows[index];
            char start_pos[32], finish_pos[32], laps[32];
            line_value(start_pos, sizeof(start_pos), r->outcome.has_start_position,
                       r->outcome.start_position);
            line_value(finish_pos, sizeof(finish_pos), r->outcome.has_finish_position,
                       r->outcome.finish_position);
            line_value(laps, sizeof(laps), r->outcome.has_laps, r->outcome.laps);
            if (!r->has_outcome)
                (void)snprintf(line, sizeof(line), "%c %s | no outcome record",
                               selected ? '>' : ' ', r->driver.name);
            else
                (void)snprintf(line, sizeof(line),
                               "%c %s | recorded | start:%s finish:%s laps:%s status:%s",
                               selected ? '>' : ' ', r->driver.name, start_pos, finish_pos, laps,
                               r->outcome.has_status ? r->outcome.status : "not recorded");
        }
        apl_tui_theme_apply(&state->theme, body,
                            selected ? APL_TUI_ROLE_SELECTION : APL_TUI_ROLE_BODY);
        apl_tui_frame_draw_line(body, (int)(start + i), line);
        apl_tui_theme_reset(body);
    }
}

void handle_events_input(shell_state *state, uint32_t id)
{
    if (state->screen == SHELL_SCREEN_OUTCOME_HISTORY && (id == 'v' || id == 'V')) {
        state->outcome_history_after = !state->outcome_history_after;
        state->outcome_history_offset = 0;
        return;
    }
    if (state->screen == SHELL_SCREEN_OUTCOME_HISTORY && (id == 'h' || id == 'H')) {
        if (state->outcome_history_offset > 0)
            --state->outcome_history_offset;
        return;
    }
    if (state->screen == SHELL_SCREEN_OUTCOME_HISTORY && (id == 'l' || id == 'L')) {
        if (state->outcome_history_offset < 4096)
            ++state->outcome_history_offset;
        return;
    }
    if (handle_body_list_navigation(state, id)) {
        if (state->screen == SHELL_SCREEN_OUTCOME_HISTORY)
            state->outcome_history_offset = 0;
        return;
    }
    apl_error error;
    apl_error_init(&error);
    if (id == NCKEY_ESC) {
        if (state->screen == SHELL_SCREEN_OUTCOME_HISTORY) {
            state->screen = SHELL_SCREEN_SESSION_DETAIL;
            apl_tui_list_set_count(&state->list, state->race_session_detail.row_count);
            apl_tui_list_select(&state->list, state->outcome_return_index);
        } else if (state->screen == SHELL_SCREEN_SESSION_DETAIL) {
            state->screen = SHELL_SCREEN_EVENT_DETAIL;
            apl_tui_list_set_count(&state->list, state->race_event_detail.session_count);
            apl_tui_list_select(&state->list, state->session_return_index);
        } else if (state->screen == SHELL_SCREEN_EVENT_DETAIL) {
            state->screen = SHELL_SCREEN_EVENT_LIST;
            apl_tui_list_set_count(&state->list, state->race_event_count);
            apl_tui_list_select(&state->list, state->event_return_index);
        } else
            set_section(state, APL_TUI_SECTION_TEAMS);
        return;
    }
    if (!is_enter_key(id))
        return;
    if (state->screen == SHELL_SCREEN_EVENT_LIST &&
        state->list.selected < state->race_event_count) {
        apl_event_detail detail = {};
        apl_status status =
            apl_event_detail_load(state->app, active_team_id(state),
                                  state->race_events[state->list.selected].id, &detail, &error);
        if (status != APL_OK) {
            show_error_dialog(state, &error);
            return;
        }
        state->event_return_index = state->list.selected;
        apl_event_detail_free(&state->race_event_detail);
        state->race_event_detail = detail;
        state->screen = SHELL_SCREEN_EVENT_DETAIL;
        apl_tui_list_set_count(&state->list, detail.session_count);
        apl_tui_list_select(&state->list, 0);
    } else if (state->screen == SHELL_SCREEN_EVENT_DETAIL &&
               state->list.selected < state->race_event_detail.session_count) {
        apl_session_detail detail = {};
        apl_status status = apl_session_detail_load(
            state->app, active_team_id(state), state->race_event_detail.event.id,
            state->race_event_detail.sessions[state->list.selected].id, &detail, &error);
        if (status != APL_OK) {
            show_error_dialog(state, &error);
            return;
        }
        state->session_return_index = state->list.selected;
        apl_session_detail_free(&state->race_session_detail);
        state->race_session_detail = detail;
        state->screen = SHELL_SCREEN_SESSION_DETAIL;
        apl_tui_list_set_count(&state->list, detail.row_count);
        apl_tui_list_select(&state->list, 0);
    } else if (state->screen == SHELL_SCREEN_SESSION_DETAIL &&
               state->list.selected < state->race_session_detail.row_count) {
        const apl_session_outcome_row *row = &state->race_session_detail.rows[state->list.selected];
        if (!row->has_outcome) {
            set_status(state, "This driver has no outcome record for this session.");
            return;
        }
        apl_outcome_correction *history = nullptr;
        size_t count = 0;
        if (apl_outcome_correction_list(&state->app->db, active_team_id(state), row->outcome.id,
                                        &history, &count, &error) != APL_OK) {
            show_error_dialog(state, &error);
            return;
        }
        state->outcome_return_index = state->list.selected;
        apl_outcome_correction_free_list(state->outcome_corrections);
        state->outcome_corrections = history;
        state->outcome_correction_count = count;
        state->outcome_history_offset = 0;
        state->outcome_history_after = false;
        state->screen = SHELL_SCREEN_OUTCOME_HISTORY;
        apl_tui_list_set_count(&state->list, count);
        apl_tui_list_select(&state->list, 0);
    }
}
