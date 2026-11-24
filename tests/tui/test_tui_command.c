#include <string.h>

#include <criterion/criterion.h>

#include "tui/command.h"

Test(tui_command, empty_and_whitespace_lines)
{
    cr_assert_eq(apl_tui_command_parse("").status, APL_TUI_COMMAND_EMPTY);
    cr_assert_eq(apl_tui_command_parse("   \t ").status, APL_TUI_COMMAND_EMPTY);
    cr_assert_eq(apl_tui_command_parse(":").status, APL_TUI_COMMAND_EMPTY);
}

Test(tui_command, view_commands_and_aliases)
{
    const apl_tui_command_parse_result view = apl_tui_command_parse(":view money");
    cr_assert_eq(view.status, APL_TUI_COMMAND_OK);
    cr_assert_eq(view.request.id, APL_TUI_COMMAND_VIEW);
    cr_assert_eq(view.request.positional_count, 1);
    cr_assert_str_eq(view.request.positionals[0], "money");

    const apl_tui_command_parse_result alias = apl_tui_command_parse("  v   teams  ");
    cr_assert_eq(alias.status, APL_TUI_COMMAND_OK);
    cr_assert_eq(alias.request.id, APL_TUI_COMMAND_VIEW);
    cr_assert_str_eq(alias.request.positionals[0], "teams");
}

Test(tui_command, overview_defaults_and_values)
{
    const apl_tui_command_parse_result active = apl_tui_command_parse("overview");
    cr_assert_eq(active.status, APL_TUI_COMMAND_OK);
    cr_assert_eq(active.request.positional_count, 0);

    const apl_tui_command_parse_result all = apl_tui_command_parse("overview all");
    cr_assert_eq(all.status, APL_TUI_COMMAND_OK);
    cr_assert_str_eq(all.request.positionals[0], "all");

    const apl_tui_command_parse_result bad = apl_tui_command_parse("overview seasons");
    cr_assert_eq(bad.status, APL_TUI_COMMAND_INVALID);
    cr_assert_neq(strstr(bad.suggestion, "active, all"), nullptr);
}

Test(tui_command, quoted_team_names_are_one_value)
{
    const apl_tui_command_parse_result result =
        apl_tui_command_parse("team switch \"Scuderia Ferrari\"");
    cr_assert_eq(result.status, APL_TUI_COMMAND_OK);
    cr_assert_eq(result.request.id, APL_TUI_COMMAND_TEAM_SWITCH);
    cr_assert_eq(result.request.positional_count, 1);
    cr_assert_str_eq(result.request.positionals[0], "Scuderia Ferrari");

    const apl_tui_command_parse_result punctuation =
        apl_tui_command_parse("team switch 'Ferrari #1 (test)'");
    cr_assert_eq(punctuation.status, APL_TUI_COMMAND_OK);
    cr_assert_str_eq(punctuation.request.positionals[0], "Ferrari #1 (test)");

    cr_assert_eq(apl_tui_command_parse("team switch Scuderia Ferrari").status,
                 APL_TUI_COMMAND_INVALID);
    cr_assert_eq(apl_tui_command_parse("team switch \"unterminated").status,
                 APL_TUI_COMMAND_INVALID);
}

Test(tui_command, team_create_completeness_and_options)
{
    const apl_tui_command_parse_result incomplete = apl_tui_command_parse("team create");
    cr_assert_eq(incomplete.status, APL_TUI_COMMAND_INCOMPLETE);
    cr_assert_eq(incomplete.request.id, APL_TUI_COMMAND_TEAM_CREATE);
    cr_assert_neq(strstr(incomplete.suggestion, "team create"), nullptr);

    const apl_tui_command_parse_result option =
        apl_tui_command_parse("team create --name \"Red Bull\" --notes \"Second team\"");
    cr_assert_eq(option.status, APL_TUI_COMMAND_OK);
    cr_assert_eq(option.request.option_count, 2);
    cr_assert_str_eq(apl_tui_command_option_value(&option.request, "name"), "Red Bull");
    cr_assert_str_eq(apl_tui_command_option_value(&option.request, "notes"), "Second team");

    const apl_tui_command_parse_result positional = apl_tui_command_parse("team create Ferrari");
    cr_assert_eq(positional.status, APL_TUI_COMMAND_OK);
    cr_assert_str_eq(positional.request.positionals[0], "Ferrari");

    const apl_tui_command_parse_result equals =
        apl_tui_command_parse("team create --name=McLaren --notes=Woking");
    cr_assert_eq(equals.status, APL_TUI_COMMAND_OK);
    cr_assert_str_eq(apl_tui_command_option_value(&equals.request, "name"), "McLaren");

    const apl_tui_command_parse_result duplicate =
        apl_tui_command_parse("team create --name Ferrari --name McLaren");
    cr_assert_eq(duplicate.status, APL_TUI_COMMAND_INVALID);

    const apl_tui_command_parse_result both =
        apl_tui_command_parse("team create Ferrari --name McLaren");
    cr_assert_eq(both.status, APL_TUI_COMMAND_INVALID);
}

Test(tui_command, option_errors_are_explicit)
{
    cr_assert_eq(apl_tui_command_parse("team create --name").status, APL_TUI_COMMAND_INVALID);
    cr_assert_eq(apl_tui_command_parse("team create --bogus x").status, APL_TUI_COMMAND_INVALID);
    cr_assert_eq(apl_tui_command_parse("team list --name x").status, APL_TUI_COMMAND_INVALID);
}

Test(tui_command, unknown_incomplete_and_invalid_are_distinguishable)
{
    const apl_tui_command_parse_result unknown = apl_tui_command_parse("SELECT * FROM teams;");
    cr_assert_eq(unknown.status, APL_TUI_COMMAND_UNKNOWN);
    cr_assert_neq(strstr(unknown.error, "Unknown command"), nullptr);

    const apl_tui_command_parse_result prefix = apl_tui_command_parse("team");
    cr_assert_eq(prefix.status, APL_TUI_COMMAND_INCOMPLETE);
    cr_assert_neq(strstr(prefix.suggestion, "team"), nullptr);

    const apl_tui_command_parse_result bad_view = apl_tui_command_parse("view seasons");
    cr_assert_eq(bad_view.status, APL_TUI_COMMAND_INVALID);
    cr_assert_neq(strstr(bad_view.suggestion, "overview"), nullptr);

    const apl_tui_command_parse_result too_many = apl_tui_command_parse("view money extra");
    cr_assert_eq(too_many.status, APL_TUI_COMMAND_INVALID);
}

Test(tui_command, aliases_and_exact_lookup)
{
    const apl_tui_command_parse_result quit = apl_tui_command_parse("q");
    cr_assert_eq(quit.status, APL_TUI_COMMAND_OK);
    cr_assert_eq(quit.request.id, APL_TUI_COMMAND_QUIT);

    cr_assert_eq(apl_tui_command_lookup("team switch")->id, APL_TUI_COMMAND_TEAM_SWITCH);
    cr_assert_eq(apl_tui_command_lookup("ts")->id, APL_TUI_COMMAND_TEAM_SWITCH);
    cr_assert_null(apl_tui_command_lookup("team"));
    cr_assert_null(apl_tui_command_lookup("ts Ferrari"));
}

Test(tui_command, help_and_completion_come_from_the_registry)
{
    char registry[APL_TUI_COMMAND_HELP_MAX];
    apl_tui_command_registry_help(registry, sizeof(registry));
    cr_assert_neq(strstr(registry, "team switch <name-or-id>"), nullptr);
    cr_assert_neq(strstr(registry, "team create [--name <name>]"), nullptr);

    const apl_tui_command_spec *const spec = apl_tui_command_lookup("team create");
    char help[APL_TUI_COMMAND_HELP_MAX];
    apl_tui_command_help(spec, help, sizeof(help));
    cr_assert_neq(strstr(help, "Usage: team create"), nullptr);
    cr_assert_neq(strstr(help, "--name <value>"), nullptr);

    char completion[APL_TUI_COMMAND_SUGGESTION_MAX];
    apl_tui_command_complete("team c", completion, sizeof(completion));
    cr_assert_neq(strstr(completion, "team create"), nullptr);
    apl_tui_command_complete(":", completion, sizeof(completion));
    cr_assert_neq(strstr(completion, "view"), nullptr);
    apl_tui_command_complete("bogus", completion, sizeof(completion));
    cr_assert_str_empty(completion);
}

Test(tui_command, status_names_are_stable)
{
    cr_assert_str_eq(apl_tui_command_status_name(APL_TUI_COMMAND_OK), "ok");
    cr_assert_str_eq(apl_tui_command_status_name(APL_TUI_COMMAND_INCOMPLETE), "incomplete");
    cr_assert_str_eq(apl_tui_command_status_name(APL_TUI_COMMAND_UNKNOWN), "unknown");
}
