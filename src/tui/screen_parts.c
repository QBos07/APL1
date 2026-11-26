#include "tui/shell_internal.h"

#include <stdio.h>
#include <string.h>

#include "domain/part.h"

/* Parts window (work items 6.8-6.12).
 *
 * The Parts section shows the derived current fitment by car/position, the
 * part register (types and individual units), and each unit's lifecycle
 * history. Non-replacement lifecycle actions (install/remove/repair/dispose)
 * and the atomic replacement use the shared lifecycle service. Filters narrow
 * the register and history; every create/edit/archive uses the part type/unit
 * use cases. */

static void copy_text(char *const out, const size_t out_size, const char *const text)
{
    (void)snprintf(out, out_size, "%s", (text != nullptr) ? text : "");
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

static void type_label(const shell_state *const state, const int64_t type_id, char *const out,
                       const size_t out_size)
{
    out[0] = '\0';
    if (type_id <= 0) {
        return;
    }
    apl_part_type type = {};
    apl_error error;
    apl_error_init(&error);
    if (apl_part_type_service_get(
            state->app, active_team_id(state), type_id, &type, &error) == APL_OK) {
        (void)snprintf(out, out_size, "%s / %s", type.name, type.category);
    } else {
        copy_text(out, out_size, "(unknown type)");
    }
}

static size_t find_part_unit_index(const shell_state *const state, const int64_t id)
{
    for (size_t index = 0; index < state->part_unit_count; ++index) {
        if (state->part_units[index].id == id) {
            return index;
        }
    }
    return SIZE_MAX;
}

static size_t find_part_type_index(const shell_state *const state, const int64_t id)
{
    for (size_t index = 0; index < state->part_type_count; ++index) {
        if (state->part_types[index].id == id) {
            return index;
        }
    }
    return SIZE_MAX;
}

apl_status load_part_units(shell_state *const state, const int64_t preferred_id,
                           apl_error *const error)
{
    int64_t keep_id = preferred_id;
    if (keep_id == 0 && state->part_unit_count > 0 &&
        state->list.selected < state->part_unit_count) {
        keep_id = state->part_units[state->list.selected].id;
    }
    apl_part_unit *units = nullptr;
    size_t count = 0;
    const int64_t team_id = active_team_id(state);
    apl_status status = APL_OK;
    if (team_id > 0) {
        apl_part_unit_filter filter = {
            .team_id = team_id,
            .include_archived = state->include_archived_part_units,
            .type_id = state->part_unit_type_filter,
        };
        status = apl_part_unit_service_list(state->app, &filter, &units, &count, error);
    }
    if (status != APL_OK) {
        return status;
    }
    apl_part_unit_service_free_list(state->part_units);
    state->part_units = units;
    state->part_unit_count = count;
    apl_tui_list_set_count(&state->list, count);
    if (keep_id != 0) {
        const size_t index = find_part_unit_index(state, keep_id);
        if (index != SIZE_MAX) {
            apl_tui_list_select(&state->list, index);
        }
    }
    return APL_OK;
}

apl_status load_part_types(shell_state *const state, const int64_t preferred_id,
                           apl_error *const error)
{
    int64_t keep_id = preferred_id;
    if (keep_id == 0 && state->part_type_count > 0 &&
        state->list.selected < state->part_type_count) {
        keep_id = state->part_types[state->list.selected].id;
    }
    apl_part_type *types = nullptr;
    size_t count = 0;
    const int64_t team_id = active_team_id(state);
    apl_status status = APL_OK;
    if (team_id > 0) {
        apl_part_type_filter filter = {
            .team_id = team_id,
            .include_archived = state->include_archived_part_types,
        };
        copy_text(filter.category_key, sizeof(filter.category_key),
                  state->part_type_category_filter);
        status = apl_part_type_service_list(state->app, &filter, &types, &count, error);
    }
    if (status != APL_OK) {
        return status;
    }
    apl_part_type_service_free_list(state->part_types);
    state->part_types = types;
    state->part_type_count = count;
    apl_tui_list_set_count(&state->list, count);
    if (keep_id != 0) {
        const size_t index = find_part_type_index(state, keep_id);
        if (index != SIZE_MAX) {
            apl_tui_list_select(&state->list, index);
        }
    }
    return APL_OK;
}

apl_status load_parts_fitment(shell_state *const state, apl_error *const error)
{
    const int64_t team_id = active_team_id(state);
    if (team_id <= 0) {
        return APL_OK;
    }
    apl_fitment_row *rows = nullptr;
    size_t count = 0;
    apl_status status =
        apl_lifecycle_service_current_fitment(state->app, team_id, &rows, &count, error);
    if (status != APL_OK) {
        return status;
    }
    apl_lifecycle_service_free_fitment(state->fitment_rows);
    state->fitment_rows = rows;
    state->fitment_count = count;
    apl_tui_list_set_count(&state->list, count);
    return APL_OK;
}

apl_status load_part_unit_detail(shell_state *const state, const int64_t unit_id,
                                 apl_error *const error)
{
    const int64_t team_id = active_team_id(state);
    if (team_id <= 0 || unit_id <= 0) {
        return APL_OK;
    }
    apl_part_unit unit = {};
    apl_status status = apl_part_unit_service_get(state->app, team_id, unit_id, &unit, error);
    if (status != APL_OK) {
        return status;
    }
    apl_lifecycle_history_entry *events = nullptr;
    size_t count = 0;
    status = apl_lifecycle_service_list_unit(state->app, team_id, unit_id, &events, &count, error);
    if (status != APL_OK) {
        return status;
    }
    state->part_unit_detail = unit;
    apl_lifecycle_service_free_history(state->unit_events);
    state->unit_events = events;
    state->unit_event_count = count;
    apl_tui_list_set_count(&state->list, count);
    return APL_OK;
}

void open_part_unit_detail(shell_state *const state, apl_part_unit unit)
{
    apl_error error;
    apl_error_init(&error);
    const int64_t team_id = active_team_id(state);
    if (team_id <= 0) {
        set_status_warning(state, "Select an active team before opening a part unit.");
        return;
    }
    if (load_part_unit_detail(state, unit.id, &error) != APL_OK) {
        show_error_dialog(state, &error);
        return;
    }
    state->section = APL_TUI_SECTION_PARTS;
    state->screen = SHELL_SCREEN_PART_UNIT_DETAIL;
    state->dialog_part_unit_id = unit.id;
    sync_status_context(state);
    set_status(state,
               "i install | r remove | p repair | d dispose | R replace | Esc register.");
}

void open_selected_part_unit(shell_state *const state)
{
    if (state->part_unit_count == 0 || state->list.selected >= state->part_unit_count) {
        return;
    }
    open_part_unit_detail(state, state->part_units[state->list.selected]);
}

void draw_parts_fitment(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    if (active_team_id(state) == 0) {
        apl_tui_frame_draw_line(body, 0, "No active team. Select a team before browsing parts.");
        return;
    }
    char header[128];
    (void)snprintf(header, sizeof(header), "Current recorded fitment: %zu row%s",
                   state->fitment_count, state->fitment_count == 1 ? "" : "s");
    apl_tui_frame_draw_line(body, 0, header);

    apl_tui_frame_draw_line(body, 1,
                            "Fitment is derived from recorded lifecycle events; no stock or "
                            "availability is assumed.");

    if (state->fitment_count == 0) {
        apl_tui_frame_draw_line(
            body, 3,
            "No unit is currently recorded as fitted. Open a unit and install it to record a "
            "fitment.");
        return;
    }
    const apl_tui_layout *const layout = apl_tui_frame_layout(&state->frame);
    const unsigned rows = (layout != nullptr) ? layout->body.rows : 0;
    const size_t visible = apl_tui_list_visible_count(&state->list);
    for (size_t offset = 0; offset < visible && offset + 4 < rows - 1; ++offset) {
        const size_t index = state->list.offset + offset;
        const apl_fitment_row *const row = &state->fitment_rows[index];
        const bool focused = index == state->list.selected;
        char car[APL_REFERENCE_LABEL_MAX];
        char type[128];
        car_label(state, row->car_id, car, sizeof(car));
        type_label(state, row->type_id, type, sizeof(type));
        char line[512];
        (void)snprintf(line, sizeof(line), "%c %-20.20s %-12.12s  unit %lld  %s", focused ? '>' : ' ',
                       car[0] != '\0' ? car : "(unknown car)", row->position,
                       (long long)row->unit_id, type);
        apl_tui_theme_apply(&state->theme, body,
                            focused ? APL_TUI_ROLE_SELECTION : APL_TUI_ROLE_BODY);
        apl_tui_frame_draw_line(body, (int)(offset + 4), line);
        apl_tui_theme_reset(body);
    }
}

void draw_part_type_list(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    if (active_team_id(state) == 0) {
        apl_tui_frame_draw_line(body, 0, "No active team. Select a team before browsing parts.");
        return;
    }
    char header[256];
    (void)snprintf(header, sizeof(header), "Part types: %zu%s", state->part_type_count,
                   state->part_type_category_filter[0] != '\0' ? " (filtered)" : "");
    apl_tui_frame_draw_line(body, 0, header);
    if (state->part_type_count == 0) {
        apl_tui_frame_draw_line(
            body, 2, "No part types yet. Press n to create your own type; nothing is seeded.");
        return;
    }
    const size_t visible = apl_tui_list_visible_count(&state->list);
    for (size_t offset = 0; offset < visible; ++offset) {
        const size_t index = state->list.offset + offset;
        const apl_part_type *const type = &state->part_types[index];
        const bool focused = index == state->list.selected;
        char line[256];
        (void)snprintf(line, sizeof(line), "%c %.64s  [%s]%s", focused ? '>' : ' ', type->name,
                       type->category, type->meta.archived ? "  [archived]" : "");
        apl_tui_theme_apply(&state->theme, body,
                            focused ? APL_TUI_ROLE_SELECTION : APL_TUI_ROLE_BODY);
        apl_tui_frame_draw_line(body, (int)(offset + 2), line);
        apl_tui_theme_reset(body);
    }
}

void draw_part_unit_list(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    if (active_team_id(state) == 0) {
        apl_tui_frame_draw_line(body, 0, "No active team. Select a team before browsing parts.");
        return;
    }
    apl_tui_frame_draw_line(body, 0, "Individual part units:");
    if (state->part_unit_count == 0) {
        apl_tui_frame_draw_line(body, 2,
                                "No part units. Press n to create your own unit; no inventory is "
                                "assumed.");
        return;
    }
    const size_t visible = apl_tui_list_visible_count(&state->list);
    for (size_t offset = 0; offset < visible; ++offset) {
        const size_t index = state->list.offset + offset;
        const apl_part_unit *const unit = &state->part_units[index];
        const bool focused = index == state->list.selected;
        char type[128];
        type_label(state, unit->type_id, type, sizeof(type));
        char line[256];
        (void)snprintf(line, sizeof(line), "%c #%lld  %.48s  %.96s%.48s", focused ? '>' : ' ',
                       (long long)unit->id,
                       unit->serial_number[0] != '\0' ? unit->serial_number : "(no serial)", type,
                       unit->part_number);
        apl_tui_theme_apply(&state->theme, body,
                            focused ? APL_TUI_ROLE_SELECTION : APL_TUI_ROLE_BODY);
        apl_tui_frame_draw_line(body, (int)(offset + 2), line);
        apl_tui_theme_reset(body);
    }
}

void draw_part_unit_detail(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    const apl_tui_layout *const layout = apl_tui_frame_layout(&state->frame);
    const unsigned rows = (layout != nullptr) ? layout->body.rows : 0;
    const apl_part_unit *const unit = &state->part_unit_detail;
    char type[128];
    type_label(state, unit->type_id, type, sizeof(type));
    int row = 0;
    char line[512];
    (void)snprintf(line, sizeof(line), "Part unit #%lld%s  %s", (long long)unit->id,
                   unit->meta.archived ? " [archived]" : "",
                   unit->serial_number[0] != '\0' ? unit->serial_number : "(no serial)");
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Type: %s", type);
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Part number: %s",
                   unit->part_number[0] != '\0' ? unit->part_number : "not recorded");
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Condition: %s",
                   unit->condition[0] != '\0' ? unit->condition : "not recorded");
    apl_tui_frame_draw_line(body, row++, line);
    apl_tui_frame_draw_line(body, row++, "Notes:");
    row += (int)apl_tui_frame_draw_wrapped(
        body, row, 2, layout != nullptr && layout->body.cols > 2 ? layout->body.cols - 2 : 40,
        unit->notes[0] != '\0' ? unit->notes : "not recorded");
    ++row;

    apl_tui_frame_draw_line(body, row++, "Lifecycle history (entered-date order):");
    if (state->unit_event_count == 0) {
        apl_tui_frame_draw_line(body, row++, "  No lifecycle events recorded.");
    } else {
        for (size_t index = 0; index < state->unit_event_count && row < (int)rows - 1; ++index) {
            const apl_lifecycle_history_entry *const entry = &state->unit_events[index];
            (void)snprintf(line, sizeof(line), "  %c %s  %s  %s%s%s",
                           entry->current ? '*' : ' ', entry->event.event_date,
                           apl_lifecycle_action_label(entry->event.action),
                           entry->event.position[0] != '\0' ? "front: " : "",
                           entry->event.position,
                           entry->event.reason[0] != '\0' ? "  reason: " : "");
            apl_tui_frame_draw_line(body, row++, line);
            if (entry->event.reason[0] != '\0' && row < (int)rows - 1) {
                (void)snprintf(line, sizeof(line), "      %.400s", entry->event.reason);
                apl_tui_frame_draw_line(body, row++, line);
            }
        }
    }
    if (row < (int)rows - 1) {
        (void)snprintf(line, sizeof(line), "Created: %s   Updated: %s", unit->meta.created_at,
                       unit->meta.updated_at);
        apl_tui_frame_draw_line(body, row++, line);
    }
}

void handle_parts_fitment_input(shell_state *const state, const uint32_t id)
{
    apl_error error;
    apl_error_init(&error);
    if (handle_body_list_navigation(state, id)) {
        return;
    }
    switch (id) {
    case NCKEY_ESC:
        set_section(state, APL_TUI_SECTION_OVERVIEW);
        return;
    case 'n':
    case 'N':
        if (active_team_id(state) == 0) {
            set_status_warning(state, "Select an active team before creating a unit.");
        } else {
            state->screen = SHELL_SCREEN_PART_UNIT_LIST;
            if (load_part_units(state, 0, &error) != APL_OK) {
                show_error_dialog(state, &error);
            } else {
                open_part_unit_dialog(state, false);
            }
        }
        return;
    default:
        break;
    }
}

void handle_part_type_list_input(shell_state *const state, const uint32_t id)
{
    apl_error error;
    apl_error_init(&error);
    if (handle_body_list_navigation(state, id)) {
        return;
    }
    switch (id) {
    case NCKEY_ESC:
        set_section(state, APL_TUI_SECTION_OVERVIEW);
        return;
    case 'n':
    case 'N':
        if (active_team_id(state) == 0) {
            set_status_warning(state, "Select an active team before creating a part type.");
        } else {
            open_part_type_dialog(state, false);
        }
        return;
    case 'a':
    case 'A':
        state->include_archived_part_types = !state->include_archived_part_types;
        if (load_part_types(state, 0, &error) != APL_OK) {
            show_error_dialog(state, &error);
        } else {
            set_status(state, state->include_archived_part_types ? "Including archived part types."
                                                                  : "Hiding archived part types.");
        }
        return;
    case 'f':
    case 'F':
        open_parts_filter_dialog(state);
        return;
    default:
        break;
    }
    if (is_enter_key(id)) {
        if (state->part_type_count > 0 && state->list.selected < state->part_type_count) {
            open_part_type_detail(state, state->part_types[state->list.selected]);
        }
    }
}

void open_part_type_detail(shell_state *const state, const apl_part_type type)
{
    apl_error error;
    apl_error_init(&error);
    state->part_type_detail = type;
    state->section = APL_TUI_SECTION_PARTS;
    state->screen = SHELL_SCREEN_PART_TYPE_DETAIL;
    state->dialog_part_type_id = type.id;
    sync_status_context(state);
    set_status(state, "e edit | x archive | n add unit | Esc types.");
}

void draw_part_type_detail(shell_state *const state)
{
    struct ncplane *const body = apl_tui_frame_plane(&state->frame, APL_TUI_REGION_BODY);
    if (body == nullptr) {
        return;
    }
    const apl_part_type *const type = &state->part_type_detail;
    int row = 0;
    char line[512];
    (void)snprintf(line, sizeof(line), "Part type: %s%s", type->name,
                   type->meta.archived ? " [archived]" : "");
    apl_tui_frame_draw_line(body, row++, line);
    (void)snprintf(line, sizeof(line), "Category: %s", type->category);
    apl_tui_frame_draw_line(body, row++, line);
    apl_tui_frame_draw_line(body, row++, "Description:");
    row += (int)apl_tui_frame_draw_wrapped(
        body, row, 2, 60, type->description[0] != '\0' ? type->description : "not recorded");
    apl_tui_frame_draw_line(body, row++, "Notes:");
    row += (int)apl_tui_frame_draw_wrapped(
        body, row, 2, 60, type->notes[0] != '\0' ? type->notes : "not recorded");
    if (row < 20) {
        (void)snprintf(line, sizeof(line), "Created: %s   Updated: %s", type->meta.created_at,
                       type->meta.updated_at);
        apl_tui_frame_draw_line(body, row++, line);
    }
}

void handle_part_type_detail_input(shell_state *const state, const uint32_t id)
{
    apl_error error;
    apl_error_init(&error);
    switch (id) {
    case NCKEY_ESC:
        state->screen = SHELL_SCREEN_PART_TYPE_LIST;
        if (load_part_types(state, state->part_type_detail.id, &error) != APL_OK) {
            show_error_dialog(state, &error);
        }
        return;
    case 'e':
    case 'E':
        if (state->part_type_detail.meta.archived) {
            set_status_warning(state, "Archived part types are read-only in this view.");
        } else {
            open_part_type_dialog(state, true);
        }
        return;
    case 'x':
    case 'X':
        if (state->part_type_detail.meta.archived) {
            set_status_warning(state, "This part type is already archived.");
        } else {
            state->pending = SHELL_PENDING_ARCHIVE_PART_TYPE;
            state->pending_record_id = state->part_type_detail.id;
            apl_tui_dialog_open_confirm(
                &state->dialog, "Archive part type?",
                "The type and its existing units/history remain readable. No records are deleted.");
            apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
        }
        return;
    case 'n':
    case 'N':
        if (active_team_id(state) == 0) {
            set_status_warning(state, "Select an active team before creating a unit.");
        } else {
            state->screen = SHELL_SCREEN_PART_UNIT_LIST;
            state->part_unit_type_filter = state->part_type_detail.id;
            if (load_part_units(state, 0, &error) != APL_OK) {
                show_error_dialog(state, &error);
            } else {
                open_part_unit_dialog(state, false);
            }
        }
        return;
    default:
        break;
    }
}

void handle_part_unit_list_input(shell_state *const state, const uint32_t id)
{
    apl_error error;
    apl_error_init(&error);
    if (handle_body_list_navigation(state, id)) {
        return;
    }
    switch (id) {
    case NCKEY_ESC:
        set_section(state, APL_TUI_SECTION_OVERVIEW);
        return;
    case 'n':
    case 'N':
        if (active_team_id(state) == 0) {
            set_status_warning(state, "Select an active team before creating a part unit.");
        } else {
            open_part_unit_dialog(state, false);
        }
        return;
    case 'a':
    case 'A':
        state->include_archived_part_units = !state->include_archived_part_units;
        if (load_part_units(state, 0, &error) != APL_OK) {
            show_error_dialog(state, &error);
        } else {
            set_status(state, state->include_archived_part_units ? "Including archived part units."
                                                                  : "Hiding archived part units.");
        }
        return;
    default:
        break;
    }
    if (is_enter_key(id)) {
        open_selected_part_unit(state);
    }
}

void handle_part_unit_detail_input(shell_state *const state, const uint32_t id)
{
    apl_error error;
    apl_error_init(&error);
    if (handle_body_list_navigation(state, id)) {
        return;
    }
    switch (id) {
    case NCKEY_ESC:
        state->screen = SHELL_SCREEN_PART_UNIT_LIST;
        if (load_part_units(state, state->part_unit_detail.id, &error) != APL_OK) {
            show_error_dialog(state, &error);
        }
        return;
    case 'e':
    case 'E':
        if (state->part_unit_detail.meta.archived) {
            set_status_warning(state, "Archived part units are read-only in this view.");
        } else {
            open_part_unit_dialog(state, true);
        }
        return;
    case 'i':
    case 'I':
        open_lifecycle_dialog(state, APL_LIFECYCLE_INSTALL);
        return;
    case 'r':
        open_lifecycle_dialog(state, APL_LIFECYCLE_REMOVE);
        return;
    case 'R':
        open_replace_dialog_direct(state);
        return;
    case 'p':
    case 'P':
        open_lifecycle_dialog(state, APL_LIFECYCLE_REPAIR);
        return;
    case 'd':
    case 'D':
        open_lifecycle_dialog(state, APL_LIFECYCLE_DISPOSE);
        return;
    case 'x':
    case 'X':
        if (state->part_unit_detail.meta.archived) {
            set_status_warning(state, "This part unit is already archived.");
        } else {
            state->pending = SHELL_PENDING_ARCHIVE_PART_UNIT;
            state->pending_record_id = state->part_unit_detail.id;
            apl_tui_dialog_open_confirm(
                &state->dialog, "Archive part unit?",
                "The unit and its lifecycle history remain readable. No records are deleted.");
            apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
        }
        return;
    default:
        break;
    }
}

void build_parts_filter_bar(const shell_state *const state, char *const out, const size_t out_size,
                            unsigned starts[6], unsigned ends[6])
{
    out[0] = '\0';
    unsigned cursor = 0;
    char label[160];
    (void)snprintf(label, sizeof(label), "Action:%s",
                   state->parts_action_filter[0] != '\0' ? state->parts_action_filter : "any");
    append_filter_button(out, out_size, &cursor, starts, ends, 0, label);
    (void)snprintf(label, sizeof(label), "From:%s",
                   state->parts_from_date[0] != '\0' ? state->parts_from_date : "any");
    append_filter_button(out, out_size, &cursor, starts, ends, 1, label);
    (void)snprintf(label, sizeof(label), "To:%s",
                   state->parts_to_date[0] != '\0' ? state->parts_to_date : "any");
    append_filter_button(out, out_size, &cursor, starts, ends, 2, label);
    (void)snprintf(label, sizeof(label), "Search:%s",
                   state->parts_search[0] != '\0' ? state->parts_search : "any");
    append_filter_button(out, out_size, &cursor, starts, ends, 3, label);
    (void)snprintf(label, sizeof(label), "Archived:%s",
                   state->include_archived_part_units ? "yes" : "no");
    append_filter_button(out, out_size, &cursor, starts, ends, 4, label);
    append_filter_button(out, out_size, &cursor, starts, ends, 5, "Clear");
}

void open_parts_filter_dialog(shell_state *const state)
{
    apl_tui_dialog_open_form(
        &state->dialog, "Filter parts",
        "Action matches install/remove/repair/dispose/replace. Dates are YYYY-MM-DD. Search matches "
        "serial/part number.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Action (optional)",
                                   state->parts_action_filter, false);
    (void)apl_tui_dialog_add_field(&state->dialog, "From date (optional)", state->parts_from_date,
                                   false);
    (void)apl_tui_dialog_add_field(&state->dialog, "To date (optional)", state->parts_to_date,
                                   false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Serial/part number (optional)",
                                   state->parts_search, false);
    state->dialog_context = SHELL_DIALOG_CONTEXT_PART_FILTER;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Enter applies the filters; Esc leaves them unchanged.");
}

bool apply_parts_filter(shell_state *const state, const char *const action, const char *const car,
                        const char *const from_date, const char *const to_date,
                        const char *const search, apl_error *const error)
{
    (void)car;
    char action_label[APL_LIFECYCLE_EVENT_MAX];
    apl_status status = apl_reference_optional_normalize(action, "action filter", action_label,
                                                         sizeof(action_label), error);
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot filter parts", error);
        return false;
    }
    char from[APL_REFERENCE_DATE_MAX];
    char to[APL_REFERENCE_DATE_MAX];
    char text[APL_PART_UNIT_SERIAL_MAX];
    status = apl_reference_date_normalize(from_date, "from date", from, sizeof(from), error);
    if (status == APL_OK) {
        status = apl_reference_date_normalize(to_date, "to date", to, sizeof(to), error);
    }
    if (status == APL_OK) {
        status = apl_reference_optional_normalize(search, "search", text, sizeof(text), error);
    }
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot filter parts", error);
        return false;
    }
    copy_text(state->parts_action_filter, sizeof(state->parts_action_filter), action_label);
    copy_text(state->parts_from_date, sizeof(state->parts_from_date), from);
    copy_text(state->parts_to_date, sizeof(state->parts_to_date), to);
    copy_text(state->parts_search, sizeof(state->parts_search), text);
    if (load_part_units(state, 0, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    set_status_success(state, "Parts filters applied.");
    return true;
}

/* Resolve a unit by stable ID entered as text. */
apl_status part_resolve_unit_by_id(shell_state *const state, const char *const text,
                                   int64_t *const out, apl_error *const error)
{
    *out = 0;
    if (text == nullptr || text[0] == '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "a part unit is required");
        apl_error_set_field(error, APL_FIELD_NEW_UNIT);
        return APL_ERR_INVALID_ARGUMENT;
    }
    char *endptr = nullptr;
    const long long value = strtoll(text, &endptr, 10);
    if (endptr == text || *endptr != '\0' || value <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part unit must be a stable ID number");
        apl_error_set_field(error, APL_FIELD_NEW_UNIT);
        return APL_ERR_INVALID_ARGUMENT;
    }
    const int64_t team_id = active_team_id(state);
    apl_part_unit unit = {};
    apl_status result =
        apl_part_unit_service_get(state->app, team_id, value, &unit, error);
    if (result != APL_OK) {
        apl_error_set_field(error, APL_FIELD_NEW_UNIT);
        return result;
    }
    *out = value;
    return APL_OK;
}

apl_status part_resolve_car_by_name(shell_state *const state, const char *const text,
                                    int64_t *const out, apl_error *const error)
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

apl_status part_resolve_type_id_for_dialog(shell_state *const state, const char *const text,
                                           int64_t *const out, apl_error *const error)
{
    *out = 0;
    if (text == nullptr || text[0] == '\0') {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "a part type ID is required");
        apl_error_set_field(error, APL_FIELD_CATEGORY);
        return APL_ERR_INVALID_ARGUMENT;
    }
    char *endptr = nullptr;
    const long long value = strtoll(text, &endptr, 10);
    if (endptr == text || *endptr != '\0' || value <= 0) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "part type must be a stable ID number");
        apl_error_set_field(error, APL_FIELD_CATEGORY);
        return APL_ERR_INVALID_ARGUMENT;
    }
    const int64_t team_id = active_team_id(state);
    apl_part_type type = {};
    const apl_status status =
        apl_part_type_service_get(state->app, team_id, value, &type, error);
    if (status != APL_OK) {
        apl_error_set_field(error, APL_FIELD_CATEGORY);
        return status;
    }
    *out = value;
    return APL_OK;
}

/* Forms opened from shell_dialogs.c; these helpers build the dialogs. */

void open_part_type_dialog(shell_state *const state, const bool edit)
{
    const apl_part_type *const type = edit ? &state->part_type_detail : nullptr;
    apl_tui_dialog_open_form(
        &state->dialog, edit ? "Edit part type" : "Create part type",
        "Category and name are required, user-entered labels.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Category", type != nullptr ? type->category : "",
                                   true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Name", type != nullptr ? type->name : "", true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Description (optional)",
                                   type != nullptr ? type->description : "", false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes (optional)",
                                   type != nullptr ? type->notes : "", false);
    state->dialog_context = edit ? SHELL_DIALOG_CONTEXT_PART_TYPE_EDIT
                                 : SHELL_DIALOG_CONTEXT_PART_TYPE_CREATE;
    state->dialog_part_type_id = edit ? type->id : 0;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter saves; Esc cancels without writing.");
}

void open_part_unit_dialog(shell_state *const state, const bool edit)
{
    const apl_part_unit *const unit = edit ? &state->part_unit_detail : nullptr;
    char type_text[64] = "";
    if (unit != nullptr) {
        (void)snprintf(type_text, sizeof(type_text), "%lld", (long long)unit->type_id);
    }
    apl_tui_dialog_open_form(
        &state->dialog, edit ? "Edit part unit" : "Create part unit",
        "Serial and part number are optional; a unit is still distinct by stable ID. Type is "
        "required.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Type ID", type_text, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Serial number (optional)",
                                   unit != nullptr ? unit->serial_number : "", false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Part number (optional)",
                                   unit != nullptr ? unit->part_number : "", false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Condition (optional)",
                                   unit != nullptr ? unit->condition : "", false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes (optional)", unit != nullptr ? unit->notes : "",
                                   false);
    state->dialog_context = edit ? SHELL_DIALOG_CONTEXT_PART_UNIT_EDIT
                                 : SHELL_DIALOG_CONTEXT_PART_UNIT_CREATE;
    state->dialog_part_type_id = unit != nullptr ? unit->type_id : 0;
    state->dialog_part_unit_id = edit ? unit->id : 0;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter saves; Esc cancels without writing.");
}

void open_lifecycle_dialog(shell_state *const state, const apl_lifecycle_action action)
{
    char unit_text[32];
    (void)snprintf(unit_text, sizeof(unit_text), "%lld",
                   (long long)state->part_unit_detail.id);
    const char *const title = apl_lifecycle_action_label(action);
    apl_tui_dialog_open_form(
        &state->dialog, title,
        "Install/remove require a car and position. Date is the user-entered event date.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Unit ID", unit_text, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Car (optional)", "", false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Position (optional)", "", false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Date (YYYY-MM-DD)", "", true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Event label (optional)", "", false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Reason (optional)", "", false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Usage (optional)", "", false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes (optional)", "", false);
    switch (action) {
    case APL_LIFECYCLE_INSTALL:
        state->dialog_context = SHELL_DIALOG_CONTEXT_PART_INSTALL;
        break;
    case APL_LIFECYCLE_REMOVE:
        state->dialog_context = SHELL_DIALOG_CONTEXT_PART_REMOVE;
        break;
    case APL_LIFECYCLE_REPAIR:
        state->dialog_context = SHELL_DIALOG_CONTEXT_PART_REPAIR;
        break;
    case APL_LIFECYCLE_DISPOSE:
        state->dialog_context = SHELL_DIALOG_CONTEXT_PART_DISPOSE;
        break;
    default:
        state->dialog_context = SHELL_DIALOG_CONTEXT_PART_INSTALL;
        break;
    }
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Tab moves fields. Enter records the event; Esc writes nothing.");
}

void open_replace_dialog_direct(shell_state *const state)
{
    char old_text[32];
    char new_text[32] = "";
    (void)snprintf(old_text, sizeof(old_text), "%lld", (long long)state->part_unit_detail.id);
    apl_tui_dialog_open_form(
        &state->dialog, "Replace unit",
        "Enter old (currently-fitted) and new unit IDs, an optional car/position, and a required "
        "date. You review the result before the atomic save.");
    (void)apl_tui_dialog_add_field(&state->dialog, "Old unit ID", old_text, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "New unit ID", new_text, true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Car (optional)", "", false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Position (optional)", "", false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Date (YYYY-MM-DD)", "", true);
    (void)apl_tui_dialog_add_field(&state->dialog, "Event label (optional)", "", false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Reason (optional)", "", false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Usage (optional)", "", false);
    (void)apl_tui_dialog_add_field(&state->dialog, "Notes (optional)", "", false);
    state->dialog_context = SHELL_DIALOG_CONTEXT_PART_REPLACE;
    apl_tui_focus_push(&state->focus, APL_TUI_FOCUS_DIALOG);
    set_status(state, "Enter previews the replacement; confirm the review to save atomically.");
}

unsigned part_type_form_error_field(const apl_error *const error)
{
    switch (apl_error_field_id(error)) {
    case APL_FIELD_CATEGORY:
        return 0;
    case APL_FIELD_NAME:
        return 1;
    case APL_FIELD_DESCRIPTION:
        return 2;
    case APL_FIELD_NOTES:
        return 3;
    default:
        return 0;
    }
}

unsigned part_unit_form_error_field(const apl_error *const error)
{
    switch (apl_error_field_id(error)) {
    case APL_FIELD_CATEGORY:
        return 0;
    case APL_FIELD_SERIAL_NUMBER:
        return 1;
    case APL_FIELD_PART_NUMBER:
        return 2;
    case APL_FIELD_CONDITION:
        return 3;
    case APL_FIELD_NOTES:
        return 4;
    default:
        return 0;
    }
}

unsigned lifecycle_form_error_field(const apl_error *const error)
{
    switch (apl_error_field_id(error)) {
    case APL_FIELD_NEW_UNIT:
        return 0;
    case APL_FIELD_CAR:
        return 1;
    case APL_FIELD_POSITION:
        return 2;
    case APL_FIELD_DATE:
        return 3;
    case APL_FIELD_EVENT_LABEL:
        return 4;
    case APL_FIELD_REASON:
        return 5;
    case APL_FIELD_USAGE:
        return 6;
    case APL_FIELD_NOTES:
        return 7;
    default:
        return 3;
    }
}

bool perform_part_type_create(shell_state *const state, const char *const category,
                              const char *const name, const char *const description,
                              const char *const notes, apl_error *const error)
{
    apl_part_type created = {};
    const apl_status result = apl_part_type_service_create(
        state->app, active_team_id(state), category, name, description, notes, &created, error);
    if (result != APL_OK) {
        set_status_from_error(state, "Cannot create part type", error);
        return false;
    }
    state->part_type_detail = created;
    if (load_part_types(state, created.id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    set_status_success(state, "Part type '%s' created.", created.name);
    return true;
}

bool perform_part_type_update(shell_state *const state, const char *const category,
                              const char *const name, const char *const description,
                              const char *const notes, apl_error *const error)
{
    apl_part_type updated = {};
    const apl_status result = apl_part_type_service_update(
        state->app, active_team_id(state), state->dialog_part_type_id, category, name, description,
        notes, &updated, error);
    if (result != APL_OK) {
        set_status_from_error(state, "Cannot save part type", error);
        return false;
    }
    state->part_type_detail = updated;
    if (load_part_types(state, updated.id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    set_status_success(state, "Part type changes saved.");
    return true;
}

void perform_part_type_archive(shell_state *const state, const int64_t id, apl_error *const error)
{
    apl_part_type archived = {};
    if (apl_part_type_service_archive(state->app, active_team_id(state), id, &archived, error) !=
        APL_OK) {
        show_error_dialog(state, error);
        return;
    }
    if (load_part_types(state, 0, error) != APL_OK) {
        show_error_dialog(state, error);
        return;
    }
    state->screen = SHELL_SCREEN_PART_TYPE_LIST;
    set_status_success(state, "Part type archived. Its units and history remain readable.");
}

static apl_status store_unit_draft(shell_state *const state, const char *const type_name,
                                   const char *const serial, const char *const part_number,
                                   apl_error *const error)
{
    (void)state;
    (void)type_name;
    (void)serial;
    (void)part_number;
    (void)error;
    return APL_OK;
}

bool perform_part_unit_create(shell_state *const state, const char *const type_name,
                              const char *const serial, const char *const part_number,
                              const char *const condition, const char *const notes,
                              apl_error *const error)
{
    int64_t type_id = 0;
    apl_status status = part_resolve_type_id_for_dialog(state, type_name, &type_id, error);
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot create part unit", error);
        return false;
    }
    apl_part_unit created = {};
    status = apl_part_unit_service_create(state->app, active_team_id(state), type_id, serial,
                                          part_number, condition, notes, &created, error);
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot create part unit", error);
        return false;
    }
    state->part_unit_detail = created;
    state->dialog_part_unit_id = created.id;
    if (load_part_units(state, created.id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    state->screen = SHELL_SCREEN_PART_UNIT_DETAIL;
    if (load_part_unit_detail(state, created.id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    set_status_success(state, "Part unit created (stable ID %lld).", (long long)created.id);
    return true;
}

bool perform_part_unit_update(shell_state *const state, const int64_t type_id,
                              const char *const serial, const char *const part_number,
                              const char *const condition, const char *const notes,
                              apl_error *const error)
{
    apl_part_unit updated = {};
    (void)store_unit_draft(state, "", serial, part_number, error);
    const apl_status result = apl_part_unit_service_update(
        state->app, active_team_id(state), state->dialog_part_unit_id,
        type_id > 0 ? type_id : state->dialog_part_type_id, serial, part_number, condition, notes,
        &updated, error);
    if (result != APL_OK) {
        set_status_from_error(state, "Cannot save part unit", error);
        return false;
    }
    state->part_unit_detail = updated;
    if (load_part_units(state, updated.id, error) != APL_OK ||
        load_part_unit_detail(state, updated.id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    state->screen = SHELL_SCREEN_PART_UNIT_DETAIL;
    set_status_success(state, "Part unit changes saved.");
    return true;
}

void perform_part_unit_archive(shell_state *const state, const int64_t id, apl_error *const error)
{
    apl_part_unit archived = {};
    if (apl_part_unit_service_archive(state->app, active_team_id(state), id, &archived, error) !=
        APL_OK) {
        show_error_dialog(state, error);
        return;
    }
    if (load_part_units(state, 0, error) != APL_OK) {
        show_error_dialog(state, error);
        return;
    }
    state->screen = SHELL_SCREEN_PART_UNIT_LIST;
    set_status_success(state, "Part unit archived. Its history remains readable.");
}

bool perform_lifecycle_action(shell_state *const state, const apl_lifecycle_action action,
                              const char *const unit, const char *const car,
                              const char *const position, const char *const event_date,
                              const char *const event_label, const char *const reason,
                              const char *const usage, const char *const notes,
                              apl_error *const error)
{
    int64_t unit_id = 0;
    apl_status status = part_resolve_unit_by_id(state, unit, &unit_id, error);
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot record lifecycle action", error);
        return false;
    }
    int64_t car_id = 0;
    status = part_resolve_car_by_name(state, car, &car_id, error);
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot record lifecycle action", error);
        return false;
    }
    apl_lifecycle_event stored = {};
    switch (action) {
    case APL_LIFECYCLE_INSTALL:
        status = apl_lifecycle_service_install(
            state->app, active_team_id(state), unit_id, car_id, position, event_date, event_label,
            reason, usage, notes, &stored, error);
        break;
    case APL_LIFECYCLE_REMOVE:
        status = apl_lifecycle_service_remove(
            state->app, active_team_id(state), unit_id, car_id, position, event_date, event_label,
            reason, usage, notes, &stored, error);
        break;
    case APL_LIFECYCLE_REPAIR:
        status = apl_lifecycle_service_repair(
            state->app, active_team_id(state), unit_id, car_id, position, event_date, event_label,
            reason, usage, notes, &stored, error);
        break;
    case APL_LIFECYCLE_DISPOSE:
        status = apl_lifecycle_service_dispose(state->app, active_team_id(state), unit_id,
                                               event_date, event_label, reason, usage, notes,
                                               &stored, error);
        break;
    default:
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "unknown lifecycle action");
        return false;
    }
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot record lifecycle action", error);
        return false;
    }
    if (load_part_units(state, 0, error) != APL_OK ||
        load_part_unit_detail(state, unit_id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    state->screen = SHELL_SCREEN_PART_UNIT_DETAIL;
    set_status_success(state, "%s recorded on %s.",
                       apl_lifecycle_action_label(action), stored.event_date);
    return true;
}

bool perform_replace(shell_state *const state, const char *const old_unit, const char *const new_unit,
                     const char *const car, const char *const position, const char *const event_date,
                     const char *const event_label, const char *const reason, const char *const usage,
                     const char *const notes, apl_error *const error)
{
    int64_t old_id = 0;
    int64_t new_id = 0;
    apl_status status = part_resolve_unit_by_id(state, old_unit, &old_id, error);
    if (status == APL_OK) {
        status = part_resolve_unit_by_id(state, new_unit, &new_id, error);
    }
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot record replacement", error);
        return false;
    }
    int64_t car_id = 0;
    status = part_resolve_car_by_name(state, car, &car_id, error);
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot record replacement", error);
        return false;
    }
    apl_lifecycle_event stored = {};
    status = apl_lifecycle_service_replace(state->app, active_team_id(state), car_id, position,
                                           old_id, new_id, event_date, event_label, reason, usage,
                                           notes, &stored, error);
    if (status != APL_OK) {
        set_status_from_error(state, "Cannot record replacement", error);
        return false;
    }
    state->part_unit_detail.id = new_id;
    if (load_part_units(state, 0, error) != APL_OK ||
        load_part_unit_detail(state, new_id, error) != APL_OK) {
        show_error_dialog(state, error);
        return false;
    }
    state->screen = SHELL_SCREEN_PART_UNIT_DETAIL;
    set_status_success(state, "Replacement recorded on %s.", stored.event_date);
    return true;
}