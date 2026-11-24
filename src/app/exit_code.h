#ifndef APL_APP_EXIT_CODE_H
#define APL_APP_EXIT_CODE_H

#include "core/error.h"

/*
 * Stable process exit codes for apl1 (FH-11).
 *
 * Scripts should key off these numbers, never off stderr text. The human
 * readable message is still printed independently.
 *
 *   0  success
 *   1  application/database/runtime failure
 *   2  invalid invocation or configuration (usage)
 *   3  terminal/TUI failure
 */

typedef enum apl_exit_code {
    APL_EXIT_SUCCESS = 0,
    APL_EXIT_APPLICATION_FAILURE = 1,
    APL_EXIT_USAGE = 2,
    APL_EXIT_TERMINAL_FAILURE = 3,
} apl_exit_code;

/** Map an application status to its documented process exit code.
 *  Unknown status values map to APL_EXIT_APPLICATION_FAILURE. */
apl_exit_code apl_exit_code_for_status(apl_status status);

#endif /* APL_APP_EXIT_CODE_H */
