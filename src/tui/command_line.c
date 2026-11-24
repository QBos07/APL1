#include "tui/command_line.h"

#include <stdio.h>
#include <string.h>

#include "tui/text.h"

void apl_tui_command_line_init(apl_tui_command_line *const line)
{
    if (line == nullptr) {
        return;
    }
    *line = (apl_tui_command_line){};
}

void apl_tui_command_line_open(apl_tui_command_line *const line)
{
    if (line == nullptr) {
        return;
    }
    line->open = true;
    line->text[0] = '\0';
    line->suggestion[0] = '\0';
    line->history_cursor = line->history_count;
}

void apl_tui_command_line_close(apl_tui_command_line *const line)
{
    if (line == nullptr) {
        return;
    }
    line->open = false;
    line->text[0] = '\0';
    line->suggestion[0] = '\0';
    line->history_cursor = line->history_count;
}

bool apl_tui_command_line_is_open(const apl_tui_command_line *const line)
{
    return line != nullptr && line->open;
}

void apl_tui_command_line_insert(apl_tui_command_line *const line, const char *const text)
{
    if (line == nullptr || text == nullptr || !line->open) {
        return;
    }
    (void)apl_tui_text_append(line->text, sizeof(line->text), text);
    line->history_cursor = line->history_count;
}

void apl_tui_command_line_backspace(apl_tui_command_line *const line)
{
    if (line == nullptr || !line->open) {
        return;
    }
    (void)apl_tui_text_backspace(line->text);
}

void apl_tui_command_line_set_suggestion(apl_tui_command_line *const line,
                                         const char *const suggestion)
{
    if (line == nullptr || !line->open) {
        return;
    }
    (void)snprintf(line->suggestion, sizeof(line->suggestion), "%s",
                   (suggestion != nullptr) ? suggestion : "");
}

void apl_tui_command_line_add_history(apl_tui_command_line *const line, const char *const text)
{
    if (line == nullptr || text == nullptr || text[0] == '\0') {
        return;
    }
    if (line->history_count >= APL_TUI_COMMAND_LINE_HISTORY_MAX) {
        for (unsigned index = 1; index < APL_TUI_COMMAND_LINE_HISTORY_MAX; ++index) {
            memcpy(line->history[index - 1], line->history[index], sizeof(line->history[index]));
        }
        --line->history_count;
    }
    (void)snprintf(line->history[line->history_count], sizeof(line->history[line->history_count]),
                   "%s", text);
    ++line->history_count;
    line->history_cursor = line->history_count;
}

void apl_tui_command_line_history_previous(apl_tui_command_line *const line)
{
    if (line == nullptr || !line->open || line->history_count == 0 || line->history_cursor == 0) {
        return;
    }
    --line->history_cursor;
    (void)snprintf(line->text, sizeof(line->text), "%s", line->history[line->history_cursor]);
}

void apl_tui_command_line_history_next(apl_tui_command_line *const line)
{
    if (line == nullptr || !line->open || line->history_cursor >= line->history_count) {
        return;
    }
    ++line->history_cursor;
    if (line->history_cursor >= line->history_count) {
        line->text[0] = '\0';
    } else {
        (void)snprintf(line->text, sizeof(line->text), "%s", line->history[line->history_cursor]);
    }
}

bool apl_tui_command_line_empty(const apl_tui_command_line *const line)
{
    if (line == nullptr) {
        return true;
    }
    const char *cursor = line->text;
    while (*cursor == ' ' || *cursor == '\t') {
        ++cursor;
    }
    return *cursor == '\0';
}

size_t apl_tui_command_line_render(const apl_tui_command_line *const line, const unsigned columns,
                                   char *const out, const size_t out_size)
{
    if (out == nullptr || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    if (line == nullptr || columns == 0) {
        return 0;
    }
    char rendered[APL_TUI_COMMAND_LINE_TEXT_MAX + APL_TUI_COMMAND_LINE_SUGGESTION_MAX + 8];
    (void)snprintf(rendered, sizeof(rendered), ":%s%s%s", line->text,
                   line->suggestion[0] != '\0' ? "   " : "",
                   line->suggestion[0] != '\0' ? line->suggestion : "");
    (void)snprintf(out, out_size, "%s", rendered);
    const size_t max_bytes = (columns < out_size - 1) ? columns : out_size - 1;
    size_t length = apl_tui_text_truncate_utf8(out, max_bytes);
    while (length > 0 && out[length - 1] == ' ') {
        out[--length] = '\0';
    }
    return length;
}
