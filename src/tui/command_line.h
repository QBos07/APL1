#ifndef APL_TUI_COMMAND_LINE_H
#define APL_TUI_COMMAND_LINE_H

#include <stddef.h>

/*
 * Ex-style command-line surface state (work item 2.25).
 *
 * The command line is an input surface, not a parser. It owns the current text,
 * a session-local history, and an inline completion suggestion; the shell maps
 * Enter to the parser/action layer. Escape discards the text without executing
 * anything. No history is persisted, audited, or written to the database.
 */

static constexpr size_t APL_TUI_COMMAND_LINE_TEXT_MAX = 512;
static constexpr size_t APL_TUI_COMMAND_LINE_SUGGESTION_MAX = 256;
static constexpr size_t APL_TUI_COMMAND_LINE_HISTORY_MAX = 20;

typedef struct apl_tui_command_line {
    bool open;
    char text[APL_TUI_COMMAND_LINE_TEXT_MAX];
    char suggestion[APL_TUI_COMMAND_LINE_SUGGESTION_MAX];
    char history[APL_TUI_COMMAND_LINE_HISTORY_MAX][APL_TUI_COMMAND_LINE_TEXT_MAX];
    unsigned history_count;
    unsigned history_cursor;
} apl_tui_command_line;

/** Reset to closed with no history. */
void apl_tui_command_line_init(apl_tui_command_line *line);

/** Open an empty prompt, preserving session history. */
void apl_tui_command_line_open(apl_tui_command_line *line);

/** Close/cancel the prompt and discard the current text (history remains). */
void apl_tui_command_line_close(apl_tui_command_line *line);

/** True while the prompt owns keyboard input. */
bool apl_tui_command_line_is_open(const apl_tui_command_line *line);

/** Append typed text (a literal colon is ordinary text). */
void apl_tui_command_line_insert(apl_tui_command_line *line, const char *text);

/** Remove one UTF-8 code point. */
void apl_tui_command_line_backspace(apl_tui_command_line *line);

/** Replace the current prompt text (used when accepting a completion). */
void apl_tui_command_line_set_text(apl_tui_command_line *line, const char *text);

/** Replace the inline suggestion (empty clears it). */
void apl_tui_command_line_set_suggestion(apl_tui_command_line *line, const char *suggestion);

/** Add a submitted non-empty command to the session history. */
void apl_tui_command_line_add_history(apl_tui_command_line *line, const char *text);

/** Recall the previous/next history entry. */
void apl_tui_command_line_history_previous(apl_tui_command_line *line);
void apl_tui_command_line_history_next(apl_tui_command_line *line);

/** True when the current prompt text is empty/whitespace. */
bool apl_tui_command_line_empty(const apl_tui_command_line *line);

/** Render ":<text>" plus an inline suggestion, bounded to `columns`. */
size_t apl_tui_command_line_render(const apl_tui_command_line *line, unsigned columns, char *out,
                                   size_t out_size);

#endif /* APL_TUI_COMMAND_LINE_H */
