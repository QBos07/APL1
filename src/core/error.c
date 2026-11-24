#include "core/error.h"

#include <stdio.h>

void apl_error_init(apl_error *const error)
{
    if (error == nullptr) {
        return;
    }
    error->code = APL_OK;
    error->message[0] = '\0';
}

void apl_error_clear(apl_error *const error)
{
    apl_error_init(error);
}

void apl_error_setv(apl_error *const error, const apl_status code, const char *fmt, va_list args)
{
    if (error == nullptr) {
        return;
    }
    if (fmt == nullptr) {
        fmt = "(no description)";
    }
    error->code = code;
    (void)vsnprintf(error->message, sizeof(error->message), fmt, args);
}

void apl_error_set(apl_error *const error, const apl_status code, const char *const fmt, ...)
{
    va_list args;
    va_start(args);
    apl_error_setv(error, code, fmt, args);
    va_end(args);
}

apl_status apl_error_code(const apl_error *const error)
{
    if (error == nullptr) {
        return APL_ERR_INVALID_ARGUMENT;
    }
    return error->code;
}

const char *apl_error_message(const apl_error *const error)
{
    if (error == nullptr) {
        return "no error value supplied";
    }
    return error->message;
}

bool apl_error_is_success(const apl_error *const error)
{
    return apl_error_code(error) == APL_OK;
}

const char *apl_status_string(const apl_status status)
{
    switch (status) {
    case APL_OK:
        return "APL_OK";
    case APL_ERR_UNKNOWN:
        return "APL_ERR_UNKNOWN";
    case APL_ERR_INVALID_ARGUMENT:
        return "APL_ERR_INVALID_ARGUMENT";
    case APL_ERR_NOT_FOUND:
        return "APL_ERR_NOT_FOUND";
    case APL_ERR_IO:
        return "APL_ERR_IO";
    case APL_ERR_OUT_OF_MEMORY:
        return "APL_ERR_OUT_OF_MEMORY";
    case APL_ERR_DB_OPEN:
        return "APL_ERR_DB_OPEN";
    case APL_ERR_DB_QUERY:
        return "APL_ERR_DB_QUERY";
    case APL_ERR_DB_FOREIGN_KEYS:
        return "APL_ERR_DB_FOREIGN_KEYS";
    case APL_ERR_DB_MIGRATION:
        return "APL_ERR_DB_MIGRATION";
    case APL_ERR_DB_SCHEMA_TOO_NEW:
        return "APL_ERR_DB_SCHEMA_TOO_NEW";
    case APL_ERR_TUI_INIT:
        return "APL_ERR_TUI_INIT";
    case APL_ERR_TUI_RENDER:
        return "APL_ERR_TUI_RENDER";
    case APL_ERR_TUI_INPUT:
        return "APL_ERR_TUI_INPUT";
    default:
        return "APL_STATUS_INVALID";
    }
}
