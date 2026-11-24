#include "app/exit_code.h"

apl_exit_code apl_exit_code_for_status(const apl_status status)
{
    switch (status) {
    case APL_OK:
        return APL_EXIT_SUCCESS;
    case APL_ERR_INVALID_ARGUMENT:
    case APL_ERR_NOT_FOUND:
        return APL_EXIT_USAGE;
    case APL_ERR_TUI_INIT:
    case APL_ERR_TUI_RENDER:
    case APL_ERR_TUI_INPUT:
        return APL_EXIT_TERMINAL_FAILURE;
    default:
        return APL_EXIT_APPLICATION_FAILURE;
    }
}
