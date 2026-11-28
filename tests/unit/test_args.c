#include <string.h>

#include <criterion/criterion.h>

#include "app/args.h"

/* argv models writable process arguments, not pointers into string literals. */
Test(args, default_invocation_has_no_overrides)
{
    char *const argv[] = {(char[]){"apl1"}, nullptr};

    apl_app_args args;
    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_app_args_parse(1, argv, &args, &err), APL_OK);
    cr_assert_null(args.database_path);
    cr_assert_not(args.help);
}

Test(args, database_option_captures_path)
{
    char *const argv[] = {(char[]){"apl1"}, (char[]){"--database"}, (char[]){"/tmp/teams.sqlite3"},
                          nullptr};

    apl_app_args args;
    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_app_args_parse(3, argv, &args, &err), APL_OK);
    cr_assert_str_eq(args.database_path, "/tmp/teams.sqlite3");
    cr_assert_not(args.help);
}

Test(args, database_option_equals_form_is_accepted)
{
    char *const argv[] = {(char[]){"apl1"}, (char[]){"--database=/tmp/teams.sqlite3"}, nullptr};

    apl_app_args args;
    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_app_args_parse(2, argv, &args, &err), APL_OK);
    cr_assert_str_eq(args.database_path, "/tmp/teams.sqlite3");
}

Test(args, last_database_option_wins)
{
    char *const argv[] = {
        (char[]){"apl1"},       (char[]){"--database"},       (char[]){"/tmp/one.sqlite3"},
        (char[]){"--database"}, (char[]){"/tmp/two.sqlite3"}, nullptr};

    apl_app_args args;
    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_app_args_parse(5, argv, &args, &err), APL_OK);
    cr_assert_str_eq(args.database_path, "/tmp/two.sqlite3");
}

Test(args, help_flags_are_recognized_without_touching_other_arguments)
{
    char *const long_form[] = {(char[]){"apl1"}, (char[]){"--help"}, nullptr};
    char *const short_form[] = {(char[]){"apl1"}, (char[]){"-h"}, nullptr};

    apl_app_args args;
    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_app_args_parse(2, long_form, &args, &err), APL_OK);
    cr_assert(args.help);

    cr_assert_eq(apl_app_args_parse(2, short_form, &args, &err), APL_OK);
    cr_assert(args.help);
}

Test(args, missing_database_value_is_a_usage_error)
{
    char *const no_value[] = {(char[]){"apl1"}, (char[]){"--database"}, nullptr};
    char *const empty_value[] = {(char[]){"apl1"}, (char[]){"--database"}, (char[]){""}, nullptr};
    char *const empty_equals[] = {(char[]){"apl1"}, (char[]){"--database="}, nullptr};

    apl_app_args args;
    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_app_args_parse(2, no_value, &args, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_neq(strstr(apl_error_message(&err), "non-empty database path"), nullptr);

    cr_assert_eq(apl_app_args_parse(3, empty_value, &args, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_app_args_parse(2, empty_equals, &args, &err), APL_ERR_INVALID_ARGUMENT);
}

Test(args, unknown_option_is_rejected)
{
    char *const argv[] = {(char[]){"apl1"}, (char[]){"--bogus"}, nullptr};

    apl_app_args args;
    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_app_args_parse(2, argv, &args, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_neq(strstr(apl_error_message(&err), "unknown option"), nullptr);
}

Test(args, unexpected_positional_argument_is_rejected)
{
    char *const argv[] = {(char[]){"apl1"}, (char[]){"list"}, nullptr};

    apl_app_args args;
    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_app_args_parse(2, argv, &args, &err), APL_ERR_INVALID_ARGUMENT);
    cr_assert_neq(strstr(apl_error_message(&err), "unexpected argument"), nullptr);
}

Test(args, null_output_is_rejected)
{
    char *const argv[] = {(char[]){"apl1"}, nullptr};
    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_app_args_parse(1, argv, nullptr, &err), APL_ERR_INVALID_ARGUMENT);
}
