#include "tui/status.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "core/text.h"
#include "tui/text.h"

constexpr size_t STATUS_LINE_MAX = 768;
constexpr unsigned HINT_MIN_COLUMNS = 60;
constexpr unsigned FULL_HINT_MIN_COLUMNS = 100;

void apl_tui_status_init(apl_tui_status *const status)
{
    if (status == nullptr) {
        return;
    }
    *status = (apl_tui_status){
        .level = APL_TUI_STATUS_INFO,
    };
}

void apl_tui_status_set_team(apl_tui_status *const status, const char *const team)
{
    if (status == nullptr) {
        return;
    }
    if (team == nullptr || team[0] == '\0') {
        apl_tui_status_clear_team(status);
        return;
    }
    apl_text_copy(status->team, sizeof(status->team), team);
    status->has_team = true;
}

void apl_tui_status_clear_team(apl_tui_status *const status)
{
    if (status == nullptr) {
        return;
    }
    status->team[0] = '\0';
    status->has_team = false;
}

void apl_tui_status_set_season(apl_tui_status *const status, const char *const season)
{
    if (status == nullptr) {
        return;
    }
    if (season == nullptr || season[0] == '\0') {
        apl_tui_status_clear_season(status);
        return;
    }
    apl_text_copy(status->season, sizeof(status->season), season);
    status->has_season = true;
}

void apl_tui_status_clear_season(apl_tui_status *const status)
{
    if (status == nullptr) {
        return;
    }
    status->season[0] = '\0';
    status->has_season = false;
}

void apl_tui_status_set_context(apl_tui_status *const status, const char *const context)
{
    if (status == nullptr) {
        return;
    }
    apl_text_copy(status->context, sizeof(status->context), context);
}

bool apl_tui_status_set_message(apl_tui_status *const status, const apl_tui_status_level level,
                                const char *const format, ...)
{
    if (status == nullptr || format == nullptr) {
        return false;
    }

    va_list args;
    va_start(args);
    const bool blocked = status->message[0] != '\0' && status->level == APL_TUI_STATUS_ERROR &&
                         level != APL_TUI_STATUS_ERROR;
    if (!blocked) {
        (void)vsnprintf(status->message, sizeof(status->message), format, args);
        status->level = level;
    }
    va_end(args);
    return !blocked;
}

void apl_tui_status_clear_message(apl_tui_status *const status)
{
    if (status == nullptr) {
        return;
    }
    status->message[0] = '\0';
    status->level = APL_TUI_STATUS_INFO;
}

const char *apl_tui_status_level_marker(const apl_tui_status_level level)
{
    switch (level) {
    case APL_TUI_STATUS_SUCCESS:
        return "[ok] ";
    case APL_TUI_STATUS_WARNING:
        return "[!] ";
    case APL_TUI_STATUS_ERROR:
        return "[error] ";
    case APL_TUI_STATUS_INFO:
        break;
    }
    return "[i] ";
}

const char *apl_tui_status_level_name(const apl_tui_status_level level)
{
    switch (level) {
    case APL_TUI_STATUS_SUCCESS:
        return "success";
    case APL_TUI_STATUS_WARNING:
        return "warning";
    case APL_TUI_STATUS_ERROR:
        return "error";
    case APL_TUI_STATUS_INFO:
        break;
    }
    return "info";
}

/** Append " · segment" to the rendered line. */
static void append_segment(char *const line, const size_t capacity, const char *const segment)
{
    if (segment == nullptr || segment[0] == '\0') {
        return;
    }
    if (line[0] != '\0') {
        (void)apl_tui_text_append(line, capacity, " \xc2\xb7 ");
    }
    (void)apl_tui_text_append(line, capacity, segment);
}

size_t apl_tui_status_render(const apl_tui_status *const status, const unsigned columns,
                             char *const out, const size_t out_size)
{
    if (out == nullptr || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    if (status == nullptr || columns == 0) {
        return 0;
    }

    const bool narrow = columns < HINT_MIN_COLUMNS;
    char line[STATUS_LINE_MAX] = "";
    append_segment(line, sizeof(line), status->has_team ? status->team : "(no active team)");
    if (status->has_season && status->season[0] != '\0') {
        append_segment(line, sizeof(line), status->season);
    }
    if (!narrow) {
        append_segment(line, sizeof(line), status->context);
    }

    char message[APL_TUI_STATUS_MESSAGE_MAX + 16] = "";
    if (status->message[0] != '\0') {
        (void)snprintf(message, sizeof(message), "%s%s", apl_tui_status_level_marker(status->level),
                       status->message);
    }

    const char *hint = nullptr;
    if (columns >= FULL_HINT_MIN_COLUMNS) {
        hint = ": commands \xc2\xb7 m menu \xc2\xb7 t team \xc2\xb7 , . views";
    } else if (columns >= HINT_MIN_COLUMNS) {
        hint = ": commands";
    }

    bool message_rendered = false;
    if (message[0] != '\0') {
        if (!narrow && hint != nullptr) {
            const size_t prefix = strlen(line) + 3; /* " · " */
            const size_t tail = 3 + strlen(hint);   /* " · " + hint */
            const size_t usable = (columns > prefix + tail + 16) ? columns - prefix - tail : 0;
            if (usable >= 16) {
                char shortened[APL_TUI_STATUS_MESSAGE_MAX + 16];
                (void)snprintf(shortened, sizeof(shortened), "%s", message);
                (void)apl_tui_text_truncate_utf8(shortened, usable);
                append_segment(line, sizeof(line), shortened);
                append_segment(line, sizeof(line), hint);
                hint = nullptr;
                message_rendered = true;
            }
        }
        if (!message_rendered) {
            append_segment(line, sizeof(line), message);
        }
    }
    if (hint != nullptr) {
        append_segment(line, sizeof(line), hint);
    }

    (void)snprintf(out, out_size, "%s", line);
    const size_t max_bytes = (columns < out_size - 1) ? columns : out_size - 1;
    return apl_tui_text_truncate_utf8(out, max_bytes);
}

size_t apl_tui_status_team_hit_width(const apl_tui_status *const status, const unsigned columns)
{
    if (status == nullptr || columns == 0) {
        return 0;
    }
    const char *const team = status->has_team ? status->team : "(no active team)";
    const size_t length = strlen(team);
    return (length <= columns) ? length : columns;
}

size_t apl_tui_status_command_hit_start(const apl_tui_status *const status, const unsigned columns)
{
    char line[STATUS_LINE_MAX];
    (void)apl_tui_status_render(status, columns, line, sizeof(line));
    const char *const hint = strstr(line, ": commands");
    return (hint != nullptr) ? (size_t)(hint - line) : columns;
}
