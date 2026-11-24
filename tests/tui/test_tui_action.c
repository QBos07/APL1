#include <criterion/criterion.h>

#include "tui/action.h"

static apl_tui_command_request parse_request(const char *line)
{
    const apl_tui_command_parse_result result = apl_tui_command_parse(line);
    cr_assert_eq(result.status, APL_TUI_COMMAND_OK);
    return result.request;
}

Test(tui_action, view_and_overview_are_navigation_only)
{
    apl_error error;
    apl_error_init(&error);
    apl_tui_action action = {};

    apl_tui_command_request request = parse_request("view money");
    cr_assert_eq(apl_tui_action_from_request(&request, &action, &error), APL_OK);
    cr_assert_eq(action.kind, APL_TUI_ACTION_SWITCH_SECTION);
    cr_assert_eq(action.section, APL_TUI_SECTION_MONEY);

    request = parse_request("overview");
    cr_assert_eq(apl_tui_action_from_request(&request, &action, &error), APL_OK);
    cr_assert_eq(action.kind, APL_TUI_ACTION_SWITCH_SECTION);
    cr_assert_eq(action.section, APL_TUI_SECTION_OVERVIEW);

    request = parse_request("overview all");
    cr_assert_eq(apl_tui_action_from_request(&request, &action, &error), APL_OK);
    cr_assert_eq(action.kind, APL_TUI_ACTION_OVERVIEW_ALL);

    request = parse_request("view settings");
    cr_assert_eq(apl_tui_action_from_request(&request, &action, &error), APL_OK);
    cr_assert_eq(action.section, APL_TUI_SECTION_SETTINGS);
}

Test(tui_action, team_commands_map_to_shared_intents)
{
    apl_error error;
    apl_error_init(&error);
    apl_tui_action action = {};

    apl_tui_command_request request = parse_request("team list");
    cr_assert_eq(apl_tui_action_from_request(&request, &action, &error), APL_OK);
    cr_assert_eq(action.kind, APL_TUI_ACTION_OPEN_TEAM_LIST);

    request = parse_request("team switch \"Scuderia Ferrari\"");
    cr_assert_eq(apl_tui_action_from_request(&request, &action, &error), APL_OK);
    cr_assert_eq(action.kind, APL_TUI_ACTION_TEAM_SWITCH_TARGET);
    cr_assert_str_eq(action.value, "Scuderia Ferrari");

    request = parse_request("team create Ferrari --notes \"Historic team\"");
    cr_assert_eq(apl_tui_action_from_request(&request, &action, &error), APL_OK);
    cr_assert_eq(action.kind, APL_TUI_ACTION_CREATE_TEAM_DIRECT);
    cr_assert_str_eq(action.value, "Ferrari");
    cr_assert_str_eq(action.notes, "Historic team");

    request = parse_request("team create --name \"Red Bull\"");
    cr_assert_eq(apl_tui_action_from_request(&request, &action, &error), APL_OK);
    cr_assert_eq(action.kind, APL_TUI_ACTION_CREATE_TEAM_DIRECT);
    cr_assert_str_eq(action.value, "Red Bull");
    cr_assert_str_empty(action.notes);
}

Test(tui_action, help_about_version_and_quit)
{
    apl_error error;
    apl_error_init(&error);
    apl_tui_action action = {};

    apl_tui_command_request request = parse_request("help team switch");
    cr_assert_eq(apl_tui_action_from_request(&request, &action, &error), APL_OK);
    cr_assert_eq(action.kind, APL_TUI_ACTION_OPEN_HELP);
    cr_assert_str_eq(action.value, "team switch");

    request = parse_request("about");
    cr_assert_eq(apl_tui_action_from_request(&request, &action, &error), APL_OK);
    cr_assert_eq(action.kind, APL_TUI_ACTION_OPEN_ABOUT);

    request = parse_request("version");
    cr_assert_eq(apl_tui_action_from_request(&request, &action, &error), APL_OK);
    cr_assert_eq(action.kind, APL_TUI_ACTION_OPEN_VERSION);

    request = parse_request("quit");
    cr_assert_eq(apl_tui_action_from_request(&request, &action, &error), APL_OK);
    cr_assert_eq(action.kind, APL_TUI_ACTION_QUIT);
}

Test(tui_action, empty_or_unknown_requests_are_rejected)
{
    apl_error error;
    apl_error_init(&error);
    apl_tui_action action = {};
    apl_tui_command_request request = {.id = APL_TUI_COMMAND_NONE};

    cr_assert_eq(apl_tui_action_from_request(&request, &action, &error), APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_tui_action_from_request(nullptr, &action, &error), APL_ERR_INVALID_ARGUMENT);
    cr_assert_str_eq(apl_tui_action_name(APL_TUI_ACTION_TEAM_SWITCH_TARGET), "team-switch-target");
}
