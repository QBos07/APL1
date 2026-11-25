#include <string.h>

#include <criterion/criterion.h>

#include "core/error.h"

Test(error, init_reports_success_with_empty_message)
{
    apl_error err;
    apl_error_init(&err);

    cr_assert_eq(apl_error_code(&err), APL_OK);
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_NONE);
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
    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_NONE);
    cr_assert(apl_error_is_success(&err));
    cr_assert_str_empty(apl_error_message(&err));
}

Test(error, field_annotation_survives_context_prefixing)
{
    apl_error err;
    apl_error_init(&err);
    apl_error_set(&err, APL_ERR_INVALID_ARGUMENT, "label is required");
    apl_error_set_field(&err, APL_FIELD_LABEL);

    apl_error_prefix(&err, APL_ERR_INVALID_ARGUMENT, "cannot create car");

    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_LABEL);
    cr_assert_str_eq(apl_error_message(&err), "cannot create car: label is required");
}

Test(error, setting_a_new_error_clears_the_field_annotation)
{
    apl_error err;
    apl_error_init(&err);
    apl_error_set(&err, APL_ERR_INVALID_ARGUMENT, "label is required");
    apl_error_set_field(&err, APL_FIELD_LABEL);

    apl_error_set(&err, APL_ERR_IO, "disk failure");

    cr_assert_eq(apl_error_field_id(&err), APL_FIELD_NONE);
}

Test(error, oversize_message_is_truncated_and_terminated)
{
    apl_error err;
    apl_error_init(&err);

    char long_detail[APL_ERROR_MESSAGE_MAX * 2];
    memset(long_detail, 'x', sizeof(long_detail));
    long_detail[sizeof(long_detail) - 1] = '\0';

    apl_error_set(&err, APL_ERR_IO, "path too long: %s", long_detail);

    cr_assert_eq(apl_error_code(&err), APL_ERR_IO);
    cr_assert_lt(strlen(apl_error_message(&err)), APL_ERROR_MESSAGE_MAX);
    cr_assert_str_neq(apl_error_message(&err), long_detail);
}

Test(error, getters_defend_against_null)
{
    cr_assert_eq(apl_error_code(nullptr), APL_ERR_INVALID_ARGUMENT);
    cr_assert_eq(apl_error_field_id(nullptr), APL_FIELD_NONE);
    cr_assert_not(apl_error_is_success(nullptr));
    cr_assert_str_eq(apl_error_message(nullptr), "no error value supplied");
}

Test(error, prefix_adds_context_before_original_detail)
{
    apl_error err;
    apl_error_init(&err);
    apl_error_set(&err, APL_ERR_DB_QUERY, "no such table: teams");

    apl_error_prefix(&err, APL_ERR_DB_MIGRATION, "migration to version 2 failed");

    cr_assert_eq(apl_error_code(&err), APL_ERR_DB_MIGRATION);
    cr_assert_str_eq(apl_error_message(&err),
                     "migration to version 2 failed: no such table: teams");
}

Test(error, prefix_on_empty_message_uses_context_only)
{
    apl_error err;
    apl_error_init(&err);

    apl_error_prefix(&err, APL_ERR_DB_MIGRATION, "migration failed");

    cr_assert_eq(apl_error_code(&err), APL_ERR_DB_MIGRATION);
    cr_assert_str_eq(apl_error_message(&err), "migration failed");
}

Test(error, prefix_with_null_or_empty_context_keeps_detail)
{
    apl_error err;
    apl_error_init(&err);
    apl_error_set(&err, APL_ERR_IO, "cannot write file");

    apl_error_prefix(&err, APL_ERR_DB_MIGRATION, nullptr);
    cr_assert_eq(apl_error_code(&err), APL_ERR_DB_MIGRATION);
    cr_assert_str_eq(apl_error_message(&err), "cannot write file");

    apl_error_prefix(&err, APL_ERR_DB_MIGRATION, "");
    cr_assert_eq(apl_error_code(&err), APL_ERR_DB_MIGRATION);
    cr_assert_str_eq(apl_error_message(&err), "cannot write file");
}

Test(error, prefix_accepts_null_error_without_crashing)
{
    apl_error_prefix(nullptr, APL_ERR_DB_MIGRATION, "ignored");
    cr_assert(true);
}

Test(error, prefix_truncates_long_combined_message_safely)
{
    apl_error err;
    apl_error_init(&err);

    char long_detail[APL_ERROR_MESSAGE_MAX];
    memset(long_detail, 'd', sizeof(long_detail) - 1);
    long_detail[sizeof(long_detail) - 1] = '\0';
    apl_error_set(&err, APL_ERR_DB_QUERY, "%s", long_detail);

    apl_error_prefix(&err, APL_ERR_DB_MIGRATION, "migration failed");

    const char *const message = apl_error_message(&err);
    cr_assert_lt(strlen(message), APL_ERROR_MESSAGE_MAX);
    cr_assert_eq(strncmp(message, "migration failed: ", strlen("migration failed: ")), 0);
    cr_assert_eq(message[APL_ERROR_MESSAGE_MAX - 1], '\0');
}

Test(error, prefix_handles_context_aliasing_the_current_message)
{
    apl_error err;
    apl_error_init(&err);
    apl_error_set(&err, APL_ERR_DB_QUERY, "disk I/O error");

    apl_error_prefix(&err, APL_ERR_DB_MIGRATION, apl_error_message(&err));

    cr_assert_str_eq(apl_error_message(&err), "disk I/O error: disk I/O error");
}

Test(error, prefix_handles_repeated_prefixing)
{
    apl_error err;
    apl_error_init(&err);
    apl_error_set(&err, APL_ERR_DB_QUERY, "disk I/O error");

    apl_error_prefix(&err, APL_ERR_DB_MIGRATION, "migration 3");
    apl_error_prefix(&err, APL_ERR_DB_MIGRATION, "startup");

    cr_assert_str_eq(apl_error_message(&err), "startup: migration 3: disk I/O error");
}

Test(error, status_strings_are_stable)
{
    cr_assert_str_eq(apl_status_string(APL_OK), "APL_OK");
    cr_assert_str_eq(apl_status_string(APL_ERR_DB_MIGRATION), "APL_ERR_DB_MIGRATION");
    cr_assert_str_eq(apl_status_string((apl_status)9999), "APL_STATUS_INVALID");
}
