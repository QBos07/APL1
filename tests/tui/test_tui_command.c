#include <stdio.h>
#include <string.h>

#include <criterion/criterion.h>

#include "core/util.h"
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

Test(tui_command, view_and_overview_accept_unambiguous_prefixes)
{
    const apl_tui_command_parse_result teams = apl_tui_command_parse("view t");
    cr_assert_eq(teams.status, APL_TUI_COMMAND_OK);
    cr_assert_str_eq(teams.request.positionals[0], "teams");

    const apl_tui_command_parse_result money = apl_tui_command_parse("v m");
    cr_assert_eq(money.status, APL_TUI_COMMAND_OK);
    cr_assert_str_eq(money.request.positionals[0], "money");

    const apl_tui_command_parse_result people = apl_tui_command_parse("view peo");
    cr_assert_eq(people.status, APL_TUI_COMMAND_OK);
    cr_assert_str_eq(people.request.positionals[0], "people");

    const apl_tui_command_parse_result ambiguous = apl_tui_command_parse("view p");
    cr_assert_eq(ambiguous.status, APL_TUI_COMMAND_INVALID);
    cr_assert_neq(strstr(ambiguous.error, "Ambiguous"), nullptr);
    cr_assert_neq(strstr(ambiguous.suggestion, "people"), nullptr);
    cr_assert_neq(strstr(ambiguous.suggestion, "parts"), nullptr);

    const apl_tui_command_parse_result all = apl_tui_command_parse("overview al");
    cr_assert_eq(all.status, APL_TUI_COMMAND_OK);
    cr_assert_str_eq(all.request.positionals[0], "all");
}

Test(tui_command, direct_view_shortcuts_parse_as_view_commands)
{
    const char *const shortcuts[] = {"teams",       "people", "events",  "money",
                                     "development", "parts",  "history", "settings"};
    for (size_t index = 0; index < APL_COUNT_OF(shortcuts); ++index) {
        char line[32];
        (void)snprintf(line, sizeof(line), ":%s", shortcuts[index]);
        const apl_tui_command_parse_result result = apl_tui_command_parse(line);
        cr_assert_eq(result.status, APL_TUI_COMMAND_OK);
        cr_assert_eq(result.request.id, APL_TUI_COMMAND_VIEW);
        cr_assert_str_eq(result.request.positionals[0], shortcuts[index]);
    }

    const apl_tui_command_parse_result alias = apl_tui_command_parse(":drivers");
    cr_assert_eq(alias.status, APL_TUI_COMMAND_OK);
    cr_assert_eq(alias.request.id, APL_TUI_COMMAND_VIEW);
    cr_assert_str_eq(alias.request.positionals[0], "drivers");
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
    cr_assert_neq(strstr(registry, "Views\n"), nullptr);
    cr_assert_neq(strstr(registry, "Teams\n"), nullptr);
    cr_assert_neq(strstr(registry, "- team switch <name-or-id>  (ts)"), nullptr);
    cr_assert_neq(strstr(registry, "team create [--name NAME]"), nullptr);
    cr_assert_neq(strstr(registry, "(tl)"), nullptr);

    const apl_tui_command_spec *const spec = apl_tui_command_lookup("team create");
    char help[APL_TUI_COMMAND_HELP_MAX];
    apl_tui_command_help(spec, help, sizeof(help));
    cr_assert_neq(strstr(help, "Usage: team create"), nullptr);
    cr_assert_neq(strstr(help, "--name <value>"), nullptr);
    cr_assert_neq(strstr(help, "Alias: tc"), nullptr);

    char completion[APL_TUI_COMMAND_SUGGESTION_MAX];
    apl_tui_command_complete("team c", completion, sizeof(completion));
    cr_assert_str_eq(completion, "team create");
    apl_tui_command_complete("vi", completion, sizeof(completion));
    cr_assert_str_eq(completion, "view");
    apl_tui_command_complete("view t", completion, sizeof(completion));
    cr_assert_str_eq(completion, "view teams");
    apl_tui_command_complete("v m", completion, sizeof(completion));
    cr_assert_str_eq(completion, "v money");
    apl_tui_command_complete("view p", completion, sizeof(completion));
    cr_assert_str_eq(completion, "view people | view parts");
    apl_tui_command_complete("view ", completion, sizeof(completion));
    cr_assert_neq(strstr(completion, "view teams"), nullptr);
    apl_tui_command_complete("team s", completion, sizeof(completion));
    cr_assert_str_eq(completion, "team switch");
    apl_tui_command_complete("teams", completion, sizeof(completion));
    cr_assert_str_eq(completion, "teams");
    apl_tui_command_complete("tl", completion, sizeof(completion));
    cr_assert_str_eq(completion, "team list");
    apl_tui_command_complete("bogus", completion, sizeof(completion));
    cr_assert_str_empty(completion);
}

Test(tui_command, phase_three_reference_commands_parse)
{
    const apl_tui_command_parse_result driver_new =
        apl_tui_command_parse("driver new --name \"Alex Driver\" --short AD");
    cr_assert_eq(driver_new.status, APL_TUI_COMMAND_OK);
    cr_assert_eq(driver_new.request.id, APL_TUI_COMMAND_DRIVER_NEW);
    cr_assert_str_eq(apl_tui_command_option_value(&driver_new.request, "name"), "Alex Driver");

    const apl_tui_command_parse_result driver_form = apl_tui_command_parse("driver new");
    cr_assert_eq(driver_form.status, APL_TUI_COMMAND_OK);
    cr_assert_eq(driver_form.request.id, APL_TUI_COMMAND_DRIVER_NEW);

    cr_assert_eq(apl_tui_command_parse("driver show \"Alex Driver\"").status, APL_TUI_COMMAND_OK);
    cr_assert_eq(apl_tui_command_parse("driver show").status, APL_TUI_COMMAND_INCOMPLETE);

    const apl_tui_command_parse_result membership =
        apl_tui_command_parse("membership add --driver 1 --start 2026-01-01 --role \"Race seat\"");
    cr_assert_eq(membership.status, APL_TUI_COMMAND_OK);
    cr_assert_eq(membership.request.id, APL_TUI_COMMAND_MEMBERSHIP_ADD);
    cr_assert_str_eq(apl_tui_command_option_value(&membership.request, "start"), "2026-01-01");

    cr_assert_eq(apl_tui_command_parse("membership end --driver 1 --end 2026-06-30").status,
                 APL_TUI_COMMAND_OK);
    cr_assert_eq(apl_tui_command_parse("season select 1").request.id,
                 APL_TUI_COMMAND_SEASON_SELECT);
    cr_assert_eq(apl_tui_command_parse("car new --label SF-26").request.id,
                 APL_TUI_COMMAND_CAR_NEW);
    cr_assert_eq(apl_tui_command_parse("car show SF-26").status, APL_TUI_COMMAND_OK);
    cr_assert_eq(apl_tui_command_parse("car show").status, APL_TUI_COMMAND_INCOMPLETE);

    char completion[APL_TUI_COMMAND_SUGGESTION_MAX];
    apl_tui_command_complete("driver ", completion, sizeof(completion));
    cr_assert_neq(strstr(completion, "driver list"), nullptr);
    apl_tui_command_complete("driver l", completion, sizeof(completion));
    cr_assert_str_eq(completion, "driver list");
    apl_tui_command_complete("season n", completion, sizeof(completion));
    cr_assert_str_eq(completion, "season new");
    char help[APL_TUI_COMMAND_HELP_MAX];
    apl_tui_command_registry_help(help, sizeof(help));
    cr_assert_neq(strstr(help, "People\n"), nullptr);
    cr_assert_neq(strstr(help, "- membership add"), nullptr);
}

Test(tui_command, status_names_are_stable)
{
    cr_assert_str_eq(apl_tui_command_status_name(APL_TUI_COMMAND_OK), "ok");
    cr_assert_str_eq(apl_tui_command_status_name(APL_TUI_COMMAND_INCOMPLETE), "incomplete");
    cr_assert_str_eq(apl_tui_command_status_name(APL_TUI_COMMAND_UNKNOWN), "unknown");
}

Test(tui_command, project_and_budget_commands_parse)
{
    const apl_tui_command_parse_result project_new = apl_tui_command_parse(
        "project new --title Aero --category Body --status Open --season 2026 --car SF-26");
    cr_assert_eq(project_new.status, APL_TUI_COMMAND_OK);
    cr_assert_eq(project_new.request.id, APL_TUI_COMMAND_PROJECT_NEW);
    cr_assert_str_eq(apl_tui_command_option_value(&project_new.request, "title"), "Aero");
    cr_assert_str_eq(apl_tui_command_option_value(&project_new.request, "season"), "2026");
    cr_assert_str_eq(apl_tui_command_option_value(&project_new.request, "car"), "SF-26");

    const apl_tui_command_parse_result project_log =
        apl_tui_command_parse("project log Aero --date 2026-01-10 --summary \"First update\"");
    cr_assert_eq(project_log.status, APL_TUI_COMMAND_OK);
    cr_assert_eq(project_log.request.id, APL_TUI_COMMAND_PROJECT_LOG);
    cr_assert_str_eq(project_log.request.positionals[0], "Aero");
    cr_assert_str_eq(apl_tui_command_option_value(&project_log.request, "summary"), "First update");

    cr_assert_eq(apl_tui_command_parse("project list").request.id, APL_TUI_COMMAND_PROJECT_LIST);
    cr_assert_eq(apl_tui_command_parse("pl").request.id, APL_TUI_COMMAND_PROJECT_LIST);
    cr_assert_eq(apl_tui_command_parse("budget list").request.id, APL_TUI_COMMAND_BUDGET_LIST);
    cr_assert_eq(apl_tui_command_parse("bn").request.id, APL_TUI_COMMAND_BUDGET_NEW);
    cr_assert_eq(apl_tui_command_parse("ba").request.id, APL_TUI_COMMAND_BUDGET_ARCHIVE);
}

Test(tui_command, help_prefix_finds_project_commands)
{
    char help[APL_TUI_COMMAND_HELP_MAX];
    apl_tui_command_match_help("project", help, sizeof(help));
    cr_assert_neq(strstr(help, "project list"), nullptr);
    cr_assert_neq(strstr(help, "project new"), nullptr);
    cr_assert_neq(strstr(help, "project log"), nullptr);
    cr_assert_eq(strstr(help, "team create"), nullptr);
}
