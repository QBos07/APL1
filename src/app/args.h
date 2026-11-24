#ifndef APL_APP_ARGS_H
#define APL_APP_ARGS_H

#include <stdio.h>

#include "core/error.h"

/*
 * Command-line arguments for the interactive application (FH-12).
 *
 * Only the database override and help are part of the interactive launch.
 * This is deliberately not a headless command mode: there is no SQL, shell,
 * or general command execution here.
 */

typedef struct apl_app_args {
    /** Explicit database path from `--database`, or nullptr to keep the
     *  environment-derived default. The pointer borrows from argv. */
    const char *database_path;
    /** True when `--help`/`-h` was requested; the caller prints usage and
     *  exits without starting the TUI or touching a database. */
    bool help;
} apl_app_args;

/** Parse `argv` into `out`.
 *
 * Unknown options, missing/empty option values, and unexpected positional
 * arguments are rejected with APL_ERR_INVALID_ARGUMENT and a clear message.
 * On success `out` is fully initialized. */
apl_status apl_app_args_parse(int argc, char *const argv[], apl_app_args *out, apl_error *error);

/** Print the usage text to `stream` (stdout for `--help`, stderr for usage
 *  errors). */
void apl_app_args_print_usage(FILE *stream, const char *program_name);

#endif /* APL_APP_ARGS_H */
