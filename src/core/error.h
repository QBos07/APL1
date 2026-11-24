#ifndef APL_CORE_ERROR_H
#define APL_CORE_ERROR_H

#include <stdarg.h>

/*
 * Application error reporting for apl1.
 *
 * Conventions (see CONTRIBUTING.md):
 * - Fallible functions return an apl_status and may accept a caller-supplied
 *   apl_error* (optional: NULL is allowed) that is only written on failure.
 * - No hidden global "last error" state: every error is a value the caller owns.
 * - apl_error owns its message as a fixed-size buffer, so no dynamic allocation
 *   is involved and callers need no destructor.
 */

/** Stable application-wide status codes. Values are part of the public
 *  contract: append new codes at the end and never renumber. */
typedef enum apl_status {
    APL_OK = 0,                 /**< Success. */
    APL_ERR_UNKNOWN = 1,        /**< Failure without a more specific cause. */
    APL_ERR_INVALID_ARGUMENT = 2, /**< Caller passed an invalid value. */
    APL_ERR_NOT_FOUND = 3,      /**< A requested record/path does not exist. */
    APL_ERR_IO = 4,             /**< Filesystem/path failure (permissions, missing directory). */
    APL_ERR_OUT_OF_MEMORY = 5,  /**< Allocation failed. */
    APL_ERR_DB_OPEN = 6,        /**< SQLite connection could not be opened/initialized. */
    APL_ERR_DB_QUERY = 7,       /**< SQLite statement execution failed. */
    APL_ERR_DB_FOREIGN_KEYS = 8,/**< Foreign-key enforcement could not be enabled/verified. */
    APL_ERR_DB_MIGRATION = 9,   /**< A schema migration failed or the list is invalid. */
    APL_ERR_DB_SCHEMA_TOO_NEW = 10, /**< Database was written by a newer application. */
    APL_ERR_TUI_INIT = 11,      /**< Notcurses initialization failed. */
} apl_status;

/** Maximum length of an apl_error message, including the terminating NUL. */
#define APL_ERROR_MESSAGE_MAX 256

/** A caller-owned error value. On success paths code is APL_OK and message is
 *  empty; on failure code is one of the APL_ERR_* values and message is a
 *  concise, NUL-terminated human-readable description (truncated to fit). */
typedef struct apl_error {
    apl_status code;
    char message[APL_ERROR_MESSAGE_MAX];
} apl_error;

/** Initialize an error value to success (APL_OK, empty message). */
void apl_error_init(apl_error *error);

/** Reset an error value to the success state. Safe on initialized or cleared
 *  values; no memory is owned beyond the embedded buffer. */
void apl_error_clear(apl_error *error);

/** Fill an error value with a failure code and a printf-formatted message
 *  (truncated to APL_ERROR_MESSAGE_MAX). Ignored if error is NULL. */
void apl_error_set(apl_error *error, apl_status code, const char *fmt, ...)
        __attribute__((format(printf, 3, 4)));

/** Fill an error value using an argument list (see apl_error_set). */
void apl_error_setv(apl_error *error, apl_status code, const char *fmt, va_list args);

/** Return the stored status code (APL_OK on a cleared/initialized value). */
apl_status apl_error_code(const apl_error *error);

/** Return the stored message; empty string when the error reports success. */
const char *apl_error_message(const apl_error *error);

/** True when the error value reports success. */
bool apl_error_is_success(const apl_error *error);

/** Stable short name of a status code, for logs and tests. Unknown values
 *  return "APL_STATUS_INVALID" rather than dereferencing out-of-range data. */
const char *apl_status_string(apl_status status);

#endif /* APL_CORE_ERROR_H */
