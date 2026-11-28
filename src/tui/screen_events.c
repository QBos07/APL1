#include "tui/shell_internal.h"

#include <errno.h>

#include "db/season_repository.h"

static void open_summary_filter(shell_state *const state)
{
    apl_tui_dialog_open_form(&state->dialog, "Recorded-value summary",
                             "Current event and session are fixed. Blank filters mean all.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Season (optional)", "", false);
    (void)apl_tui_dialog_add_field(&state->dialog, "From date (YYYY-MM-DD)", "", false);
    (void)apl_tui_dialog_add_field(&state->dialog, "To date (YYYY-MM-DD)", "", false);
    state->dialog_context = SHELL_DIALOG_CONTEXT_OUTCOME_SUMMARY;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
}

bool show_outcome_summary(shell_state *const state, apl_error *const error)
{
    apl_outcome_summary_filter filter = {.team_id = active_team_id(state),
                                         .event_id = state->race_session_detail.event.id,
                                         .session_id = state->race_session_detail.session.id};
    apl_status status =
        project_resolve_season(state, state->dialog.fields[0].value, &filter.season_id, error);
    if (status != APL_OK) {
        apl_tui_dialog_set_field_error(&state->dialog, 0, apl_error_message(error));
        return false;
    }
    status = apl_reference_date_normalize(state->dialog.fields[1].value, "from date",
                                          filter.from_date, sizeof(filter.from_date), error);
    if (status != APL_OK) {
        apl_tui_dialog_set_field_error(&state->dialog, 1, apl_error_message(error));
        return false;
    }
    status = apl_reference_date_normalize(state->dialog.fields[2].value, "to date", filter.to_date,
                                          sizeof(filter.to_date), error);
    if (status == APL_OK)
        status =
            apl_reference_date_range_validate(filter.from_date, filter.to_date, "summary", error);
    if (status != APL_OK) {
        apl_tui_dialog_set_field_error(&state->dialog, 2, apl_error_message(error));
        return false;
    }
    apl_outcome_summary result = {};
    status = apl_outcome_summary_query(state->app, &filter, &result, error);
    if (status != APL_OK) {
        apl_tui_dialog_set_field_error(&state->dialog, 0, apl_error_message(error));
        return false;
    }
    char season[APL_TUI_DIALOG_FIELD_VALUE_MAX], from[APL_REFERENCE_DATE_MAX],
        to[APL_REFERENCE_DATE_MAX];
    (void)snprintf(season, sizeof(season), "%s",
                   filter.season_id ? state->dialog.fields[0].value : "all");
    (void)snprintf(from, sizeof(from), "%s", filter.from_date);
    (void)snprintf(to, sizeof(to), "%s", filter.to_date);
    char total[64];
    if (result.has_laps_total)
        (void)snprintf(total, sizeof(total), "%lld", (long long)result.laps_total);
    else
        (void)snprintf(total, sizeof(total), "not recorded");
    char message[APL_TUI_DIALOG_MESSAGE_MAX];
    (void)snprintf(message, sizeof(message),
                   "Based on recorded values only.\nTeam: %s\nEvent: %s\nSession: %s\n"
                   "Season filter: %s\nEvent date from: %s\nEvent date to: %s\n\n"
                   "Outcome records: %lld\nLaps total: %s\n"
                   "Outcomes with laps recorded: %lld\nOutcomes with laps missing: %lld\n"
                   "Missing laps are excluded, not treated as zero. No ranking or points.",
                   state->active_team.name, state->race_session_detail.event.name,
                   state->race_session_detail.session.label, season, from[0] ? from : "any",
                   to[0] ? to : "any", (long long)result.outcome_count, total,
                   (long long)result.laps_recorded_count, (long long)result.laps_missing_count);
    close_shell_dialog(state);
    apl_tui_dialog_open_error(&state->dialog, "Recorded-value summary", message);
    state->dialog_context = SHELL_DIALOG_CONTEXT_ACTIVITY_DETAIL;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    return true;
}

void clear_outcome_batch(shell_state *const state)
{
    free(state->outcome_batch);
    free(state->outcome_batch_selected);
    state->outcome_batch = nullptr;
    state->outcome_batch_selected = nullptr;
    state->outcome_batch_size = 0;
    state->outcome_batch_count = 0;
    state->outcome_batch_active = false;
}

static bool prompt_outcome_conflict(shell_state *const state, const apl_outcome *const input,
                                    const bool batch, const size_t index)
{
    apl_outcome *existing = nullptr;
    size_t count = 0;
    apl_error error;
    apl_error_init(&error);
    if (apl_outcome_repository_list(&state->app->db, input->team_id, input->event_id,
                                    input->session_id, &existing, &count, &error) != APL_OK)
        return false;
    int64_t existing_id = 0;
    for (size_t i = 0; i < count; ++i) {
        if (existing[i].driver_id == input->driver_id) {
            existing_id = existing[i].id;
            break;
        }
    }
    apl_outcome_repository_free_list(existing);
    if (existing_id == 0)
        return false;
    state->outcome_conflict_id = existing_id;
    state->outcome_conflict_index = index;
    state->outcome_conflict_batch = batch;
    if (!batch)
        state->outcome_conflict_form = state->dialog;
    apl_tui_dialog_open_choice(
        &state->dialog, "Outcome already exists",
        "This driver already has an outcome in this session. Edit existing will use your "
        "entered values (blank fields become missing). You must save again to confirm. "
        "Cancel returns without writing and keeps your values.");
    (void)apl_tui_dialog_add_option(&state->dialog, "Cancel / keep entered values", true, false);
    (void)apl_tui_dialog_add_option(&state->dialog, "Edit existing outcome", true, false);
    state->dialog_context = SHELL_DIALOG_CONTEXT_OUTCOME_CONFLICT;
    if (batch)
        apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    return true;
}

void finish_outcome_conflict(shell_state *const state, const apl_tui_dialog_result result)
{
    const bool edit =
        result == APL_TUI_DIALOG_ACCEPTED && apl_tui_dialog_selected_option(&state->dialog) == 1;
    if (state->outcome_conflict_batch) {
        if (edit && state->outcome_batch_active &&
            state->outcome_conflict_index < state->outcome_batch_size &&
            state->outcome_batch_selected[state->outcome_conflict_index]) {
            apl_outcome_batch_row *const row = &state->outcome_batch[state->outcome_conflict_index];
            row->mode = APL_OUTCOME_EDIT;
            row->input.id = state->outcome_conflict_id;
        }
        close_shell_dialog(state);
        set_status(state, edit ? "Existing outcome selected; review and save the batch again."
                               : "Batch unchanged; staged values retained.");
    } else {
        state->dialog = state->outcome_conflict_form;
        state->dialog_context = SHELL_DIALOG_CONTEXT_OUTCOME_FORM;
        if (edit) {
            state->dialog_record_id = state->outcome_conflict_id;
            (void)snprintf(state->dialog.title, sizeof(state->dialog.title),
                           "Edit existing outcome");
            set_status(state, "Existing outcome selected; review values and save again.");
        } else {
            set_status(state, "No change made; entered values retained.");
        }
    }
    state->outcome_conflict_id = 0;
}

static void begin_outcome_batch(shell_state *const state)
{
    const size_t count = state->race_session_detail.row_count;
    if (!count) {
        set_status_warning(state, "No team drivers are available for batch entry.");
        return;
    }
    apl_outcome_batch_row *const rows = calloc(count, sizeof(*rows));
    bool *const selected = calloc(count, sizeof(*selected));
    if (!rows || !selected) {
        free(rows);
        free(selected);
        set_status_warning(state, "Cannot allocate the outcome batch.");
        return;
    }
    clear_outcome_batch(state);
    state->outcome_batch = rows;
    state->outcome_batch_selected = selected;
    state->outcome_batch_size = count;
    state->outcome_batch_active = true;
    set_status(state, "Batch started. Select a driver and press a to stage values.");
}

void open_outcome_form(shell_state *const state)
{
    if (state->list.selected >= state->race_session_detail.row_count)
        return;
    const apl_session_outcome_row *const row =
        &state->race_session_detail.rows[state->list.selected];
    const apl_outcome *const outcome =
        state->outcome_batch_active && state->outcome_batch_selected[state->list.selected]
            ? &state->outcome_batch[state->list.selected].input
            : (row->has_outcome ? &row->outcome : nullptr);
    if (outcome &&
        ((outcome->has_status && strlen(outcome->status) >= APL_TUI_DIALOG_FIELD_VALUE_MAX) ||
         (outcome->has_notes && strlen(outcome->notes) >= APL_TUI_DIALOG_FIELD_VALUE_MAX))) {
        set_status_warning(state,
                           "Outcome text exceeds the form limit; editing would truncate it.");
        return;
    }
    char start[32] = "", finish[32] = "", laps[32] = "";
    if (outcome && outcome->has_start_position)
        (void)snprintf(start, sizeof(start), "%d", outcome->start_position);
    if (outcome && outcome->has_finish_position)
        (void)snprintf(finish, sizeof(finish), "%d", outcome->finish_position);
    if (outcome && outcome->has_laps)
        (void)snprintf(laps, sizeof(laps), "%d", outcome->laps);
    char message[256];
    (void)snprintf(message, sizeof(message),
                   "Driver: %.120s. Leave unknown fields blank; zero laps is a recorded value.",
                   row->driver.name);
    apl_tui_dialog_open_form(&state->dialog, outcome ? "Edit outcome" : "Enter outcome", message);
    (void)apl_tui_dialog_add_field(&state->dialog, "Start position", start, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Finish position", finish, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Laps", laps, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Status",
                                   outcome && outcome->has_status ? outcome->status : "", false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes",
                                   outcome && outcome->has_notes ? outcome->notes : "", false);
    state->dialog_context = SHELL_DIALOG_CONTEXT_OUTCOME_FORM;
    state->dialog_driver_id = row->driver.id;
    state->dialog_record_id = outcome ? outcome->id : 0;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
}

static bool parse_outcome_number(shell_state *const state, const unsigned field,
                                 const bool allow_zero, bool *const present, int *const value)
{
    const char *const text = state->dialog.fields[field].value;
    if (!text[0]) {
        *present = false;
        return true;
    }
    errno = 0;
    char *end = nullptr;
    const long parsed = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end || parsed > INT_MAX ||
        parsed < (allow_zero ? 0 : 1)) {
        apl_tui_dialog_set_field_error(&state->dialog, field,
                                       allow_zero ? "Enter a nonnegative whole number."
                                                  : "Enter a positive whole number.");
        return false;
    }
    *present = true;
    *value = (int)parsed;
    return true;
}

bool save_outcome_form(shell_state *const state, apl_error *const error)
{
    apl_outcome input = {.id = state->dialog_record_id,
                         .team_id = active_team_id(state),
                         .event_id = state->race_session_detail.event.id,
                         .session_id = state->race_session_detail.session.id,
                         .driver_id = state->dialog_driver_id};
    if (!parse_outcome_number(state, 0, false, &input.has_start_position, &input.start_position) ||
        !parse_outcome_number(state, 1, false, &input.has_finish_position,
                              &input.finish_position) ||
        !parse_outcome_number(state, 2, true, &input.has_laps, &input.laps))
        return false;
    input.has_status = state->dialog.fields[3].value[0] != '\0';
    input.has_notes = state->dialog.fields[4].value[0] != '\0';
    (void)snprintf(input.status, sizeof(input.status), "%s", state->dialog.fields[3].value);
    (void)snprintf(input.notes, sizeof(input.notes), "%s", state->dialog.fields[4].value);
    if (state->outcome_batch_active) {
        const size_t index = state->list.selected;
        if (index >= state->outcome_batch_size ||
            state->race_session_detail.rows[index].driver.id != input.driver_id) {
            apl_tui_dialog_set_field_error(&state->dialog, 0,
                                           "Batch target changed; cancel entry.");
            return false;
        }
        if (!state->outcome_batch_selected[index]) {
            state->outcome_batch_selected[index] = true;
            ++state->outcome_batch_count;
        }
        state->outcome_batch[index] = (apl_outcome_batch_row){
            .mode = input.id ? APL_OUTCOME_EDIT : APL_OUTCOME_CREATE, .input = input};
        set_status(state, "Row staged, not saved. Press r to review the batch.");
        return true;
    }
    apl_outcome saved = {};
    apl_status status = apl_outcome_service_save(
        state->app, input.id ? APL_OUTCOME_EDIT : APL_OUTCOME_CREATE, &input, &saved, error);
    if (status != APL_OK) {
        if (status == APL_ERR_CONFLICT &&
            prompt_outcome_conflict(state, &input, false, state->list.selected))
            return false;
        apl_tui_dialog_set_field_error(&state->dialog, 0, apl_error_message(error));
        return false;
    }
    apl_session_detail detail = {};
    status = apl_session_detail_load(state->app, input.team_id, input.event_id, input.session_id,
                                     &detail, error);
    if (status == APL_OK) {
        const size_t selected = state->list.selected;
        apl_session_detail_free(&state->race_session_detail);
        state->race_session_detail = detail;
        apl_tui_list_set_count(&state->list, detail.row_count);
        apl_tui_list_select(&state->list, selected);
    } else {
        set_status_warning(state, "Outcome saved, but the session could not be refreshed.");
    }
    return true;
}

void open_event_form(shell_state *const state, const bool edit)
{
    if (active_team_id(state) <= 0) {
        set_status_warning(state, "Select a team before creating an event.");
        return;
    }
    const apl_event *const event = edit ? &state->race_event_detail.event : nullptr;
    if (event && strlen(event->notes) >= APL_TUI_DIALOG_FIELD_VALUE_MAX) {
        set_status_warning(state,
                           "Event notes exceed the form limit; editing would truncate them.");
        return;
    }
    char season[APL_REFERENCE_LABEL_MAX] = "";
    if (event && event->season_id) {
        apl_season record = {};
        apl_error error;
        apl_error_init(&error);
        if (apl_season_repository_get(&state->app->db, active_team_id(state), event->season_id,
                                      &record, &error) != APL_OK) {
            show_error_dialog(state, &error);
            return;
        }
        (void)snprintf(season, sizeof(season), "%s", record.label);
    }
    apl_tui_dialog_open_form(&state->dialog, edit ? "Edit event" : "Create event",
                             "Name and date required; season is an optional same-team label.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Name", event ? event->name : "", true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Date (YYYY-MM-DD)", event ? event->date : "",
                                   true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Season (optional)", season, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Location (optional)",
                                   event ? event->location : "", false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes (optional)", event ? event->notes : "",
                                   false);
    state->dialog_context =
        edit ? SHELL_DIALOG_CONTEXT_EVENT_EDIT : SHELL_DIALOG_CONTEXT_EVENT_CREATE;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
}

void open_session_form(shell_state *const state, const bool edit)
{
    if (edit && state->list.selected >= state->race_event_detail.session_count)
        return;
    const apl_event_session *const session =
        edit ? &state->race_event_detail.sessions[state->list.selected] : nullptr;
    state->dialog_record_id = session ? session->id : 0;
    apl_tui_dialog_open_form(&state->dialog, edit ? "Edit session" : "Create session",
                             "Enter a session label; no sessions are generated.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Label", session ? session->label : "", true);
    state->dialog_context =
        edit ? SHELL_DIALOG_CONTEXT_SESSION_EDIT : SHELL_DIALOG_CONTEXT_SESSION_CREATE;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
}

bool save_event_form(shell_state *const state, apl_error *const error)
{
    const shell_dialog_context context = state->dialog_context;
    const int64_t team = active_team_id(state);
    const char *const first = state->dialog.fields[0].value;
    apl_status status;
    if (context == SHELL_DIALOG_CONTEXT_EVENT_CREATE ||
        context == SHELL_DIALOG_CONTEXT_EVENT_EDIT) {
        int64_t season = 0;
        status = project_resolve_season(state, state->dialog.fields[2].value, &season, error);
        apl_event event = {};
        if (status == APL_OK)
            status = context == SHELL_DIALOG_CONTEXT_EVENT_CREATE
                         ? apl_event_service_create(state->app, team, season, first,
                                                    state->dialog.fields[1].value,
                                                    state->dialog.fields[3].value,
                                                    state->dialog.fields[4].value, &event, error)
                         : apl_event_service_update(
                               state->app, team, state->race_event_detail.event.id, season, first,
                               state->dialog.fields[1].value, state->dialog.fields[3].value,
                               state->dialog.fields[4].value, &event, error);
        if (status != APL_OK)
            return false;
        if (context == SHELL_DIALOG_CONTEXT_EVENT_EDIT)
            state->race_event_detail.event = event;
        if (load_race_events(state, error) != APL_OK)
            set_status_warning(state, "Event saved, but the event list could not be refreshed.");
        if (context == SHELL_DIALOG_CONTEXT_EVENT_EDIT)
            apl_tui_list_set_count(&state->list, state->race_event_detail.session_count);
    } else {
        apl_event_session session = {};
        status =
            context == SHELL_DIALOG_CONTEXT_SESSION_CREATE
                ? apl_session_service_create(state->app, team, state->race_event_detail.event.id,
                                             first, &session, error)
                : apl_session_service_update(state->app, team, state->race_event_detail.event.id,
                                             state->dialog_record_id, first, &session, error);
        if (status != APL_OK)
            return false;
        apl_event_detail detail = {};
        if (apl_event_detail_load(state->app, team, state->race_event_detail.event.id, &detail,
                                  error) == APL_OK) {
            apl_event_detail_free(&state->race_event_detail);
            state->race_event_detail = detail;
            apl_tui_list_set_count(&state->list, detail.session_count);
            for (size_t i = 0; i < detail.session_count; ++i)
                if (detail.sessions[i].id == session.id)
                    apl_tui_list_select(&state->list, i);
        } else
            set_status_warning(state,
                               "Session saved, but the session list could not be refreshed.");
    }
    return true;
}

apl_status load_race_events(shell_state *const state, apl_error *const error)
{
    apl_event *events = nullptr;
    size_t count = 0;
    const int64_t team = active_team_id(state);
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

static void line_value(char *const out, const size_t size, const bool present, const int value)
{
    if (present)
        (void)snprintf(out, size, "%d", value);
    else
        (void)snprintf(out, size, "not recorded");
}

static const char *text_at_offset(const char *const text, size_t offset)
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

static void draw_history_values(struct ncplane *const body, const int row, const char *const label,
                                const apl_outcome *const value, const size_t offset)
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

static void save_outcome_batch(shell_state *const state)
{
    const size_t count = state->outcome_batch_count;
    apl_outcome_batch_row *const rows = calloc(count, sizeof(*rows));
    apl_outcome *const saved = calloc(count, sizeof(*saved));
    apl_error *const errors = calloc(count, sizeof(*errors));
    size_t *const indices = calloc(count, sizeof(*indices));
    if (!rows || !saved || !errors || !indices) {
        free(rows);
        free(saved);
        free(errors);
        free(indices);
        set_status_warning(state, "Cannot allocate batch save buffers; staged rows are unchanged.");
        return;
    }
    size_t next = 0;
    for (size_t i = 0; i < state->outcome_batch_size; ++i) {
        if (state->outcome_batch_selected[i]) {
            rows[next] = state->outcome_batch[i];
            indices[next++] = i;
        }
    }
    apl_error error;
    apl_error_init(&error);
    apl_status status =
        apl_outcome_service_save_batch(state->app, rows, count, saved, errors, &error);
    if (status != APL_OK) {
        bool prompted = false;
        for (size_t i = 0; i < count; ++i) {
            if (errors[i].code != APL_OK) {
                apl_tui_list_select(&state->list, indices[i]);
                if (errors[i].code == APL_ERR_CONFLICT)
                    prompted = prompt_outcome_conflict(state, &rows[i].input, true, indices[i]);
                break;
            }
        }
        if (!prompted)
            set_status_from_error(state, "Batch not saved; staged rows retained", &error);
    } else {
        apl_session_detail detail = {};
        status = apl_session_detail_load(state->app, active_team_id(state),
                                         state->race_session_detail.event.id,
                                         state->race_session_detail.session.id, &detail, &error);
        clear_outcome_batch(state);
        state->screen = SHELL_SCREEN_SESSION_DETAIL;
        if (status == APL_OK) {
            apl_session_detail_free(&state->race_session_detail);
            state->race_session_detail = detail;
            apl_tui_list_set_count(&state->list, detail.row_count);
            set_status_success(state, "Saved %zu outcomes atomically.", count);
        } else {
            set_status_warning(state, "Batch saved, but the session could not be refreshed.");
        }
    }
    free(rows);
    free(saved);
    free(errors);
    free(indices);
}

void draw_events_screen(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (!body)
        return;
    const apl_tui_layout *const layout = apl_tui_frame_layout(&state->frame);
    const unsigned rows = layout->body.rows;
    char line[1024];
    if (active_team_id(state) == 0) {
        apl_tui_frame_draw_line(body, 0, "No active team. Select a team to review events.");
        return;
    }
    if (state->screen == SHELL_SCREEN_OUTCOME_REVIEW) {
        const size_t index = state->list.selected;
        const apl_session_outcome_row *const driver = &state->race_session_detail.rows[index];
        (void)snprintf(line, sizeof(line), "Review %zu staged | Team: %s | Event: %s | Session: %s",
                       state->outcome_batch_count, state->active_team.name,
                       state->race_session_detail.event.name,
                       state->race_session_detail.session.label);
        apl_tui_frame_draw_line(body, 0, line);
        (void)snprintf(line, sizeof(line), "Driver %zu/%zu: %s", index + 1,
                       state->outcome_batch_size, driver->driver.name);
        apl_tui_frame_draw_line(body, 1, line);
        if (!state->outcome_batch_selected[index]) {
            apl_tui_frame_draw_line(body, 3, "Not staged; this driver will not be changed.");
        } else {
            const apl_outcome_batch_row *const row = &state->outcome_batch[index];
            apl_tui_frame_draw_line(body, 3,
                                    row->mode == APL_OUTCOME_EDIT
                                        ? "Edit existing outcome (staged, not saved)"
                                        : "Create outcome (staged, not saved)");
            draw_history_values(body, 4, "Values:", &row->input, state->outcome_history_offset);
        }
        return;
    }
    if (state->screen == SHELL_SCREEN_OUTCOME_HISTORY) {
        const apl_session_outcome_row *const row =
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
        const apl_outcome_correction *const c = &state->outcome_corrections[state->list.selected];
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
        const apl_event *const event = &state->race_event_detail.event;
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
        const size_t index = state->list.offset + i;
        if (index >= count || start + i >= rows - 1)
            break;
        const bool selected = index == state->list.selected;
        if (state->screen == SHELL_SCREEN_EVENT_LIST) {
            const apl_event *const event = &state->race_events[index];
            (void)snprintf(line, sizeof(line), "%c %s  %s", selected ? '>' : ' ', event->date,
                           event->name);
        } else if (state->screen == SHELL_SCREEN_EVENT_DETAIL) {
            (void)snprintf(line, sizeof(line), "%c %s", selected ? '>' : ' ',
                           state->race_event_detail.sessions[index].label);
        } else {
            const apl_session_outcome_row *const r = &state->race_session_detail.rows[index];
            char start_pos[32], finish_pos[32], laps[32];
            line_value(start_pos, sizeof(start_pos), r->outcome.has_start_position,
                       r->outcome.start_position);
            line_value(finish_pos, sizeof(finish_pos), r->outcome.has_finish_position,
                       r->outcome.finish_position);
            line_value(laps, sizeof(laps), r->outcome.has_laps, r->outcome.laps);
            if (state->outcome_batch_active && state->outcome_batch_selected[index])
                (void)snprintf(line, sizeof(line), "%c %s | staged (not saved)",
                               selected ? '>' : ' ', r->driver.name);
            else if (!r->has_outcome)
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

void handle_events_mouse(shell_state *const state, const uint32_t id, const unsigned row,
                         const unsigned body_rows)
{
    if (id == NCKEY_SCROLL_UP || id == NCKEY_SCROLL_DOWN) {
        apl_tui_list_move(&state->list,
                          id == NCKEY_SCROLL_UP ? -APL_TUI_SCROLL_STEP : APL_TUI_SCROLL_STEP);
        return;
    }
    if (id != NCKEY_BUTTON1 || body_rows == 0 || row >= body_rows - 1)
        return;
    if (state->screen == SHELL_SCREEN_OUTCOME_REVIEW) {
        if (row == 1)
            handle_events_input(state, NCKEY_ENTER);
        return;
    }

    unsigned start;
    size_t count;
    switch (state->screen) {
    case SHELL_SCREEN_EVENT_LIST:
        start = 2;
        count = state->race_event_count;
        break;
    case SHELL_SCREEN_EVENT_DETAIL:
        start = 4;
        count = state->race_event_detail.session_count;
        break;
    case SHELL_SCREEN_SESSION_DETAIL:
        start = 3;
        count = state->race_session_detail.row_count;
        break;
    default:
        return;
    }
    if (row < start || row - start >= state->list.viewport_rows)
        return;
    const size_t index = state->list.offset + row - start;
    if (index >= count)
        return;
    apl_tui_list_select(&state->list, index);
    if (state->screen == SHELL_SCREEN_SESSION_DETAIL &&
        (state->outcome_batch_active || !state->race_session_detail.rows[index].has_outcome))
        open_outcome_form(state);
    else
        handle_events_input(state, NCKEY_ENTER);
}

void handle_events_input(shell_state *const state, const uint32_t id)
{
    if (state->screen == SHELL_SCREEN_OUTCOME_REVIEW && (id == 's' || id == 'S')) {
        save_outcome_batch(state);
        return;
    }
    if (state->outcome_batch_active && (id == 'x' || id == 'X')) {
        clear_outcome_batch(state);
        state->screen = SHELL_SCREEN_SESSION_DETAIL;
        set_status(state, "Batch cancelled without writing.");
        return;
    }
    if (state->screen == SHELL_SCREEN_SESSION_DETAIL && !state->outcome_batch_active &&
        (id == 'b' || id == 'B')) {
        begin_outcome_batch(state);
        return;
    }
    if (state->screen == SHELL_SCREEN_SESSION_DETAIL && !state->outcome_batch_active &&
        (id == 'u' || id == 'U')) {
        open_summary_filter(state);
        return;
    }
    if (state->screen == SHELL_SCREEN_SESSION_DETAIL && state->outcome_batch_active &&
        (id == 'r' || id == 'R')) {
        if (!state->outcome_batch_count) {
            set_status_warning(state, "Stage at least one driver before review.");
        } else {
            state->screen = SHELL_SCREEN_OUTCOME_REVIEW;
            state->outcome_history_offset = 0;
        }
        return;
    }
    if (state->screen == SHELL_SCREEN_OUTCOME_REVIEW && is_enter_key(id)) {
        state->screen = SHELL_SCREEN_SESSION_DETAIL;
        open_outcome_form(state);
        return;
    }
    if (state->screen == SHELL_SCREEN_OUTCOME_REVIEW && id == NCKEY_ESC) {
        state->screen = SHELL_SCREEN_SESSION_DETAIL;
        return;
    }
    if (state->screen == SHELL_SCREEN_OUTCOME_HISTORY && (id == 'v' || id == 'V')) {
        state->outcome_history_after = !state->outcome_history_after;
        state->outcome_history_offset = 0;
        return;
    }
    if ((state->screen == SHELL_SCREEN_OUTCOME_HISTORY ||
         state->screen == SHELL_SCREEN_OUTCOME_REVIEW) &&
        (id == 'h' || id == 'H')) {
        if (state->outcome_history_offset > 0)
            --state->outcome_history_offset;
        return;
    }
    if ((state->screen == SHELL_SCREEN_OUTCOME_HISTORY ||
         state->screen == SHELL_SCREEN_OUTCOME_REVIEW) &&
        (id == 'l' || id == 'L')) {
        if (state->outcome_history_offset < 4096)
            ++state->outcome_history_offset;
        return;
    }
    if (handle_body_list_navigation(state, id)) {
        if (state->screen == SHELL_SCREEN_OUTCOME_HISTORY ||
            state->screen == SHELL_SCREEN_OUTCOME_REVIEW)
            state->outcome_history_offset = 0;
        return;
    }
    if (id == 'n' || id == 'N') {
        if (state->screen == SHELL_SCREEN_EVENT_LIST)
            open_event_form(state, false);
        else if (state->screen == SHELL_SCREEN_EVENT_DETAIL)
            open_session_form(state, false);
        return;
    }
    if ((id == 'a' || id == 'A') && state->screen == SHELL_SCREEN_SESSION_DETAIL) {
        open_outcome_form(state);
        return;
    }
    if (id == 'e' || id == 'E') {
        if (state->screen == SHELL_SCREEN_EVENT_DETAIL)
            open_event_form(state, true);
        return;
    }
    if ((id == 's' || id == 'S') && state->screen == SHELL_SCREEN_EVENT_DETAIL) {
        open_session_form(state, true);
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
            clear_outcome_batch(state);
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
        const apl_status status =
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
        const apl_status status = apl_session_detail_load(
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
        const apl_session_outcome_row *const row =
            &state->race_session_detail.rows[state->list.selected];
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
