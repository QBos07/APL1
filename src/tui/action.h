#ifndef APL_TUI_ACTION_H
#define APL_TUI_ACTION_H

#include <stdbool.h>
#include <stddef.h>

#include "core/error.h"
#include "tui/command.h"
#include "tui/navigation.h"

/*
 * Typed command intents for the Phase 2 command line (work item 2.27).
 *
 * This module only maps a parsed request to a shell action. It performs no
 * persistence and contains no SQL; the shell dispatches the action through the
 * same navigation and team use cases used by menus/dialogs.
 */

typedef enum apl_tui_action_kind {
    APL_TUI_ACTION_NONE = 0,
    APL_TUI_ACTION_SWITCH_SECTION,
    APL_TUI_ACTION_OVERVIEW_ALL,
    APL_TUI_ACTION_OPEN_TEAM_LIST,
    APL_TUI_ACTION_OPEN_TEAM_PICKER,
    APL_TUI_ACTION_OPEN_TEAM_CREATE_FORM,
    APL_TUI_ACTION_CREATE_TEAM_DIRECT,
    APL_TUI_ACTION_TEAM_SWITCH_TARGET,
    APL_TUI_ACTION_OPEN_HELP,
    APL_TUI_ACTION_OPEN_ABOUT,
    APL_TUI_ACTION_OPEN_VERSION,
    APL_TUI_ACTION_QUIT,
} apl_tui_action_kind;

typedef struct apl_tui_action {
    apl_tui_action_kind kind;
    apl_tui_section section;
    char value[APL_TUI_COMMAND_WORD_MAX];
    char notes[APL_TUI_COMMAND_WORD_MAX];
} apl_tui_action;

/** Map a successful parse result to an action.
 *
 * Returns APL_ERR_INVALID_ARGUMENT when the request id is unknown or a view
 * name cannot be resolved. */
apl_status apl_tui_action_from_request(const apl_tui_command_request *request,
                                       apl_tui_action *out_action, apl_error *error);

/** Stable action name for tests/logs. */
const char *apl_tui_action_name(apl_tui_action_kind kind);

#endif /* APL_TUI_ACTION_H */
