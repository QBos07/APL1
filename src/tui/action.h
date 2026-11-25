#ifndef APL_TUI_ACTION_H
#define APL_TUI_ACTION_H

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
    APL_TUI_ACTION_OPEN_DRIVER_LIST,
    APL_TUI_ACTION_OPEN_DRIVER_CREATE_FORM,
    APL_TUI_ACTION_CREATE_DRIVER_DIRECT,
    APL_TUI_ACTION_OPEN_DRIVER_SHOW,
    APL_TUI_ACTION_OPEN_MEMBERSHIP_ADD_FORM,
    APL_TUI_ACTION_CREATE_MEMBERSHIP_DIRECT,
    APL_TUI_ACTION_OPEN_MEMBERSHIP_END_FORM,
    APL_TUI_ACTION_END_MEMBERSHIP_DIRECT,
    APL_TUI_ACTION_OPEN_SEASON_LIST,
    APL_TUI_ACTION_OPEN_SEASON_CREATE_FORM,
    APL_TUI_ACTION_CREATE_SEASON_DIRECT,
    APL_TUI_ACTION_SEASON_SELECT_TARGET,
    APL_TUI_ACTION_OPEN_CAR_LIST,
    APL_TUI_ACTION_OPEN_CAR_CREATE_FORM,
    APL_TUI_ACTION_CREATE_CAR_DIRECT,
    APL_TUI_ACTION_OPEN_CAR_SHOW,
    APL_TUI_ACTION_OPEN_PROJECT_LIST,
    APL_TUI_ACTION_OPEN_PROJECT_CREATE_FORM,
    APL_TUI_ACTION_CREATE_PROJECT_DIRECT,
    APL_TUI_ACTION_OPEN_PROJECT_SHOW,
    APL_TUI_ACTION_OPEN_PROJECT_EDIT_FORM,
    APL_TUI_ACTION_ARCHIVE_PROJECT_TARGET,
    APL_TUI_ACTION_OPEN_PROJECT_LOG_FORM,
    APL_TUI_ACTION_LOG_PROJECT_UPDATE_DIRECT,
    APL_TUI_ACTION_OPEN_BUDGET_LIST,
    APL_TUI_ACTION_OPEN_BUDGET_CREATE_FORM,
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
    char start[APL_TUI_COMMAND_WORD_MAX];
    char end[APL_TUI_COMMAND_WORD_MAX];
    char role[APL_TUI_COMMAND_WORD_MAX];
    char driver[APL_TUI_COMMAND_WORD_MAX];
    char category[APL_TUI_COMMAND_WORD_MAX];
    char status[APL_TUI_COMMAND_WORD_MAX];
    char season[APL_TUI_COMMAND_WORD_MAX];
    char car[APL_TUI_COMMAND_WORD_MAX];
    char description[APL_TUI_COMMAND_WORD_MAX];
    char summary[APL_TUI_COMMAND_WORD_MAX];
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
