#include "tui/action.h"

#include <stdio.h>
#include <string.h>

#include "tui/text.h"

static void join_positionals(const apl_tui_command_request *const request, char *const out,
                             const size_t out_size)
{
    out[0] = '\0';
    for (unsigned index = 0; index < request->positional_count; ++index) {
        if (index > 0) {
            (void)apl_tui_text_append(out, out_size, " ");
        }
        (void)apl_tui_text_append(out, out_size, request->positionals[index]);
    }
}

apl_status apl_tui_action_from_request(const apl_tui_command_request *const request,
                                       apl_tui_action *const out_action, apl_error *const error)
{
    if (request == nullptr || out_action == nullptr) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "command action arguments are nullptr");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out_action = (apl_tui_action){};

    switch (request->id) {
    case APL_TUI_COMMAND_VIEW: {
        apl_tui_section section = APL_TUI_SECTION_COUNT;
        if (request->positional_count == 0 ||
            !apl_tui_section_from_name(request->positionals[0], &section)) {
            apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "unknown view target");
            return APL_ERR_INVALID_ARGUMENT;
        }
        out_action->kind = APL_TUI_ACTION_SWITCH_SECTION;
        out_action->section = section;
        return APL_OK;
    }
    case APL_TUI_COMMAND_OVERVIEW:
        if (request->positional_count > 0 &&
            apl_tui_text_equal_ci(request->positionals[0], "all")) {
            out_action->kind = APL_TUI_ACTION_OVERVIEW_ALL;
        } else {
            out_action->kind = APL_TUI_ACTION_SWITCH_SECTION;
            out_action->section = APL_TUI_SECTION_OVERVIEW;
        }
        return APL_OK;
    case APL_TUI_COMMAND_TEAM_LIST:
        out_action->kind = APL_TUI_ACTION_OPEN_TEAM_LIST;
        return APL_OK;
    case APL_TUI_COMMAND_TEAM_SWITCH:
        if (request->positional_count == 0) {
            out_action->kind = APL_TUI_ACTION_OPEN_TEAM_PICKER;
            return APL_OK;
        }
        out_action->kind = APL_TUI_ACTION_TEAM_SWITCH_TARGET;
        (void)snprintf(out_action->value, sizeof(out_action->value), "%s", request->positionals[0]);
        return APL_OK;
    case APL_TUI_COMMAND_TEAM_CREATE: {
        const char *const option_name = apl_tui_command_option_value(request, "name");
        const char *const name =
            (request->positional_count > 0) ? request->positionals[0] : option_name;
        if (name == nullptr || name[0] == '\0') {
            out_action->kind = APL_TUI_ACTION_OPEN_TEAM_CREATE_FORM;
            return APL_OK;
        }
        out_action->kind = APL_TUI_ACTION_CREATE_TEAM_DIRECT;
        (void)snprintf(out_action->value, sizeof(out_action->value), "%s", name);
        const char *const notes = apl_tui_command_option_value(request, "notes");
        if (notes != nullptr) {
            (void)snprintf(out_action->notes, sizeof(out_action->notes), "%s", notes);
        }
        return APL_OK;
    }
    case APL_TUI_COMMAND_HELP:
        out_action->kind = APL_TUI_ACTION_OPEN_HELP;
        join_positionals(request, out_action->value, sizeof(out_action->value));
        return APL_OK;
    case APL_TUI_COMMAND_ABOUT:
        out_action->kind = APL_TUI_ACTION_OPEN_ABOUT;
        return APL_OK;
    case APL_TUI_COMMAND_VERSION:
        out_action->kind = APL_TUI_ACTION_OPEN_VERSION;
        return APL_OK;
    case APL_TUI_COMMAND_QUIT:
        out_action->kind = APL_TUI_ACTION_QUIT;
        return APL_OK;
    case APL_TUI_COMMAND_NONE:
    case APL_TUI_COMMAND_ID_COUNT:
        break;
    }
    apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "command has no action");
    return APL_ERR_INVALID_ARGUMENT;
}

const char *apl_tui_action_name(const apl_tui_action_kind kind)
{
    switch (kind) {
    case APL_TUI_ACTION_NONE:
        return "none";
    case APL_TUI_ACTION_SWITCH_SECTION:
        return "switch-section";
    case APL_TUI_ACTION_OVERVIEW_ALL:
        return "overview-all";
    case APL_TUI_ACTION_OPEN_TEAM_LIST:
        return "open-team-list";
    case APL_TUI_ACTION_OPEN_TEAM_PICKER:
        return "open-team-picker";
    case APL_TUI_ACTION_OPEN_TEAM_CREATE_FORM:
        return "open-team-create-form";
    case APL_TUI_ACTION_CREATE_TEAM_DIRECT:
        return "create-team-direct";
    case APL_TUI_ACTION_TEAM_SWITCH_TARGET:
        return "team-switch-target";
    case APL_TUI_ACTION_OPEN_HELP:
        return "open-help";
    case APL_TUI_ACTION_OPEN_ABOUT:
        return "open-about";
    case APL_TUI_ACTION_OPEN_VERSION:
        return "open-version";
    case APL_TUI_ACTION_QUIT:
        return "quit";
    }
    return "none";
}
