#ifndef APL_TUI_STATUS_H
#define APL_TUI_STATUS_H

#include <stddef.h>

/*
 * Persistent status-bar state (work item 2.21).
 *
 * The status bar always carries the active-team identity and current view
 * context, plus the latest feedback. An unacknowledged error is never replaced
 * by an unrelated success/info message; an explicit clear or a newer error is
 * required. Rendering is terminal-free and UTF-8 safe.
 */

constexpr size_t APL_TUI_STATUS_MESSAGE_MAX = 320;
constexpr size_t APL_TUI_STATUS_CONTEXT_MAX = 64;
constexpr size_t APL_TUI_STATUS_TEAM_MAX = 128;
constexpr size_t APL_TUI_STATUS_SEASON_MAX = 96;

typedef enum apl_tui_status_level {
    APL_TUI_STATUS_INFO = 0,
    APL_TUI_STATUS_SUCCESS,
    APL_TUI_STATUS_WARNING,
    APL_TUI_STATUS_ERROR,
} apl_tui_status_level;

typedef struct apl_tui_status {
    char team[APL_TUI_STATUS_TEAM_MAX];
    bool has_team;
    char season[APL_TUI_STATUS_SEASON_MAX];
    bool has_season;
    char context[APL_TUI_STATUS_CONTEXT_MAX];
    char message[APL_TUI_STATUS_MESSAGE_MAX];
    apl_tui_status_level level;
} apl_tui_status;

/** Reset to no team, no context, and no message. */
void apl_tui_status_init(apl_tui_status *status);

/** Set/clear the active-team identity. */
void apl_tui_status_set_team(apl_tui_status *status, const char *team);
void apl_tui_status_clear_team(apl_tui_status *status);

/** Set/clear the selected season/period shown next to the team. */
void apl_tui_status_set_season(apl_tui_status *status, const char *season);
void apl_tui_status_clear_season(apl_tui_status *status);

/** Set the current view/context label (empty clears it). */
void apl_tui_status_set_context(apl_tui_status *status, const char *context);

/** Replace the message with a printf-formatted value.
 *
 * Returns false when an existing error protects the current message from an
 * unrelated lower-severity update. */
[[gnu::format(printf, 3, 4)]]
bool apl_tui_status_set_message(apl_tui_status *status, apl_tui_status_level level,
                                const char *format, ...);

/** Clear the current message. */
void apl_tui_status_clear_message(apl_tui_status *status);

/** Marker used to preserve meaning without color ("[ok]", "[!]", "[error]"). */
const char *apl_tui_status_level_marker(apl_tui_status_level level);

/** Short stable name for tests and accessible text. */
const char *apl_tui_status_level_name(apl_tui_status_level level);

/** Render the complete status line into `out`.
 *
 * `columns` is the available cell width. Narrow terminals omit the context and
 * command hint; the active team and latest message remain. The result is
 * NUL-terminated and never splits a UTF-8 sequence. Returns its length. */
size_t apl_tui_status_render(const apl_tui_status *status, unsigned columns, char *out,
                             size_t out_size);

/** Width in cells/bytes of the clickable team indicator at the start of the
 *  rendered status line. Zero when no team is shown. */
size_t apl_tui_status_team_hit_width(const apl_tui_status *status, unsigned columns);

/** Offset where the clickable command hint starts, or `columns` when the hint
 *  is not rendered at this width. */
size_t apl_tui_status_command_hit_start(const apl_tui_status *status, unsigned columns);

#endif /* APL_TUI_STATUS_H */
