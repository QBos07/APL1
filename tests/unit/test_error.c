#include <string.h>

#include <criterion/criterion.h>

#include "core/error.h"

Test(error, init_reports_success_with_empty_message)
{
    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_error_code(&err), APL_OK);
    cr_assert(apl_error_is_success(&err));
    cr_assert_str_empty(apl_error_message(&err));
}

Test(error, set_failure_captures_code_and_formatted_message)
{
    apl_error err;
    apl_error_init(&err);

    apl_error_set(&err, APL_ERR_DB_OPEN, "cannot open database at %s", "/tmp/teams.sqlite3");

    cr_assert_eq(apl_error_code(&err), APL_ERR_DB_OPEN);
    cr_assert_str_eq(apl_error_message(&err), "cannot open database at /tmp/teams.sqlite3");
    cr_assert_not(apl_error_is_success(&err));
}

Test(error, set_accepts_null_error_without_crashing)
{
    apl_error_set(nullptr, APL_ERR_IO, "ignored");

    cr_assert(true);
}

Test(error, clear_resets_a_failure_to_success)
{
    apl_error err;
    apl_error_set(&err, APL_ERR_DB_QUERY, "query failed");

    apl_error_clear(&err);

    cr_assert_eq(apl_error_code(&err), APL_OK);
    cr_assert(apl_error_is_success(&err));
    cr_assert_str_empty(apl_error_message(&err));
}

Test(error, oversize_message_is_truncated_and_terminated)
{
    apl_error err;
    apl_error_init(&err);

    char long_detail[APL_ERROR_MESSAGE_MAX * 2];
    memset(long_detail, 'x', sizeof(long_detail));

    apl_error_set(&err, APL_ERR_IO, "path too long: %s", long_detail);

    cr_assert_eq(apl_error_code(&err), APL_ERR_IO);
    cr_assert_lt(strlen(apl_error_message(&err)), APL_ERROR_MESSAGE_MAX);
    cr_assert_str_neq(apl_error_message(&err), long_detail);
}

Test(error, getters_defend_against_null)
{
    cr_assert_eq(apl_error_code(nullptr), APL_ERR_INVALID_ARGUMENT);
    cr_assert_not(apl_error_is_success(nullptr));
    cr_assert_str_eq(apl_error_message(nullptr), "no error value supplied");
}

Test(error, status_strings_are_stable)
{
    cr_assert_str_eq(apl_status_string(APL_OK), "APL_OK");
    cr_assert_str_eq(apl_status_string(APL_ERR_DB_MIGRATION), "APL_ERR_DB_MIGRATION");
    cr_assert_str_eq(apl_status_string((apl_status)9999), "APL_STATUS_INVALID");
}
