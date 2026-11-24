#include "app/args.h"

#include <string.h>

static const char *const APL_APP_USAGE_DATABASE_OPTION = "--database";

apl_status apl_app_args_parse(const int argc, char *const argv[], apl_app_args *const out,
                              apl_error *const error)
{
    if (out == nullptr || argc < 0 || (argc > 0 && argv == nullptr)) {
        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "argument parser was given invalid input");
        return APL_ERR_INVALID_ARGUMENT;
    }
    *out = (apl_app_args){};

    for (int index = 1; index < argc; ++index) {
        const char *const argument = argv[index];
        if (argument == nullptr) {
            apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "argument %d is missing", index);
            return APL_ERR_INVALID_ARGUMENT;
        }

        if (strcmp(argument, "--help") == 0 || strcmp(argument, "-h") == 0) {
            out->help = true;
            return APL_OK;
        }

        if (strcmp(argument, APL_APP_USAGE_DATABASE_OPTION) == 0) {
            if (index + 1 >= argc || argv[index + 1] == nullptr || argv[index + 1][0] == '\0') {
                apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                              "option '%s' requires a non-empty database path",
                              APL_APP_USAGE_DATABASE_OPTION);
                return APL_ERR_INVALID_ARGUMENT;
            }
            out->database_path = argv[++index];
            continue;
        }

        static const char *const database_prefix = "--database=";
        const size_t prefix_length = strlen(database_prefix);
        if (strncmp(argument, database_prefix, prefix_length) == 0) {
            const char *const value = argument + prefix_length;
            if (value[0] == '\0') {
                apl_error_set(error, APL_ERR_INVALID_ARGUMENT,
                              "option '%s' requires a non-empty database path",
                              APL_APP_USAGE_DATABASE_OPTION);
                return APL_ERR_INVALID_ARGUMENT;
            }
            out->database_path = value;
            continue;
        }

        if (argument[0] == '-' && argument[1] != '\0') {
            apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "unknown option '%s'", argument);
            return APL_ERR_INVALID_ARGUMENT;
        }

        apl_error_set(error, APL_ERR_INVALID_ARGUMENT, "unexpected argument '%s'", argument);
        return APL_ERR_INVALID_ARGUMENT;
    }
    return APL_OK;
}

void apl_app_args_print_usage(FILE *const stream, const char *const program_name)
{
    if (stream == nullptr) {
        return;
    }
    const char *const name =
            (program_name != nullptr && program_name[0] != '\0') ? program_name : "apl1";
    (void)fprintf(stream,
                  "Usage: %s [--database <path>] [--help]\n"
                  "\n"
                  "  --database <path>  Use exactly this SQLite database file.\n"
                  "  -h, --help         Show this help and exit without starting the "
                  "interface.\n"
                  "\n"
                  "Without --database, the path is resolved from XDG_DATA_HOME or HOME.\n",
                  name);
}
