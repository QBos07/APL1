#include <criterion/criterion.h>

#include "app/exit_code.h"
#include "core/util.h"

Test(exit_code, success_maps_to_zero)
{
    cr_assert_eq(apl_exit_code_for_status(APL_OK), APL_EXIT_SUCCESS);
}

Test(exit_code, invalid_invocation_or_configuration_maps_to_usage)
{
    cr_assert_eq(apl_exit_code_for_status(APL_ERR_INVALID_ARGUMENT), APL_EXIT_USAGE);
    cr_assert_eq(apl_exit_code_for_status(APL_ERR_NOT_FOUND), APL_EXIT_USAGE);
}

Test(exit_code, terminal_failures_are_distinct)
{
    cr_assert_eq(apl_exit_code_for_status(APL_ERR_TUI_INIT), APL_EXIT_TERMINAL_FAILURE);
    cr_assert_eq(apl_exit_code_for_status(APL_ERR_TUI_RENDER), APL_EXIT_TERMINAL_FAILURE);
    cr_assert_eq(apl_exit_code_for_status(APL_ERR_TUI_INPUT), APL_EXIT_TERMINAL_FAILURE);
}

Test(exit_code, application_and_database_failures_are_runtime_failures)
{
    const apl_status statuses[] = {
        APL_ERR_UNKNOWN,       APL_ERR_IO,
        APL_ERR_OUT_OF_MEMORY, APL_ERR_DB_OPEN,
        APL_ERR_DB_QUERY,      APL_ERR_DB_FOREIGN_KEYS,
        APL_ERR_DB_MIGRATION,  APL_ERR_DB_SCHEMA_TOO_NEW,
    };
    for (size_t index = 0; index < APL_COUNT_OF(statuses); ++index) {
        cr_assert_eq(apl_exit_code_for_status(statuses[index]), APL_EXIT_APPLICATION_FAILURE);
    }
}

Test(exit_code, unknown_status_is_not_reported_as_success)
{
    cr_assert_eq(apl_exit_code_for_status((apl_status)9999), APL_EXIT_APPLICATION_FAILURE);
}
