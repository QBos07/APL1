#include <string.h>

#include <criterion/criterion.h>

#include "domain/team.h"

Test(team, name_is_trimmed)
{
    apl_error err;
    apl_error_init(&err);
    char out[APL_TEAM_NAME_MAX];

    cr_assert_eq(apl_team_name_normalize("  Scuderia Ferrari  ", out, sizeof(out), &err), APL_OK);
    cr_assert_str_eq(out, "Scuderia Ferrari");
    cr_assert(apl_error_is_success(&err));
}

Test(team, blank_name_is_rejected)
{
    const char *const blanks[] = {nullptr, "", "   ", "\t\n"};

    for (size_t index = 0; index < sizeof(blanks) / sizeof(blanks[0]); ++index) {
        apl_error err;
        apl_error_init(&err);
        char out[APL_TEAM_NAME_MAX] = "unchanged";

        cr_assert_eq(apl_team_name_normalize(blanks[index], out, sizeof(out), &err),
                     APL_ERR_INVALID_ARGUMENT);
        cr_assert_neq(strstr(apl_error_message(&err), "required"), nullptr);
        cr_assert_str_eq(out, "unchanged");
    }
}

Test(team, over_long_name_is_rejected_without_truncation)
{
    apl_error err;
    apl_error_init(&err);
    char out[8] = "keep";

    cr_assert_eq(apl_team_name_normalize("12345678", out, sizeof(out), &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_neq(strstr(apl_error_message(&err), "too long"), nullptr);
    cr_assert_str_eq(out, "keep");
}

Test(team, notes_are_optional_and_trimmed)
{
    apl_error err;
    apl_error_init(&err);
    char out[APL_TEAM_NOTES_MAX];

    cr_assert_eq(apl_team_notes_normalize(nullptr, out, sizeof(out), &err), APL_OK);
    cr_assert_str_eq(out, "");

    cr_assert_eq(apl_team_notes_normalize("  headquarters notes  ", out, sizeof(out), &err),
                 APL_OK);
    cr_assert_str_eq(out, "headquarters notes");
}

Test(team, over_long_notes_are_rejected)
{
    apl_error err;
    apl_error_init(&err);
    char out[8] = "keep";

    cr_assert_eq(apl_team_notes_normalize("12345678", out, sizeof(out), &err),
                 APL_ERR_INVALID_ARGUMENT);
    cr_assert_neq(strstr(apl_error_message(&err), "too long"), nullptr);
    cr_assert_str_eq(out, "keep");
}
